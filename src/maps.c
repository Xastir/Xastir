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

//#define MAP_SCALE_CHECK


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
#include "util.h"
#include "main.h"
#include "datum.h"
#include "draw_symbols.h"
#include "rotated.h"
#include "color.h"
#include "xa_config.h"

// Must be last include file
#include "leak_detection.h"

extern XmFontList fontlist1;    // Menu/System fontlist

#define GRID_MORE 5000

#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }


// Check for XPM and/or ImageMagick.  We use "NO_GRAPHICS"
// to disable some routines below if the support for them
// is not compiled in.
#if !(defined(HAVE_LIBXPM) || defined(HAVE_LIBXPM_IN_XM) || defined(HAVE_MAGICK))
  #define NO_GRAPHICS 1
#endif  // !(HAVE_LIBXPM || HAVE_LIBXPM_IN_XM || HAVE_MAGICK)

#if !(defined(HAVE_LIBXPM) || defined(HAVE_LIBXPM_IN_XM))
  #define NO_XPM 1
#endif  // !(HAVE_LIBXPM || HAVE_LIBXPM_IN_XM)


void draw_rotated_label_text_to_target (Widget w, int rotation, int x, int y, int label_length, int color, char *label_text, int fontsize, Pixmap target_pixmap, int draw_outline, int outline_bg_color);
int get_rotated_label_text_height_pixels(Widget w, char *label_text, int fontsize);

// Constants defining the color of the labeled grid border.
int outline_border_labels = TRUE;   // if true put an outline around the border labels
int border_foreground_color = 0x20; // color of the map border, if shown
// 0x08 is black.
// 0x20 is white.
int outline_border_labels_color = 0x20;
// outline_border_labels_color = border_foreground_color;
// color of outline to draw around border labels
// use color of border to help make text more legible.

// Print options
Widget print_properties_dialog = (Widget)NULL;
static xastir_mutex print_properties_dialog_lock;
Widget print_postscript_dialog = (Widget)NULL;
static xastir_mutex print_postscript_dialog_lock;
Widget printer_data;
Widget previewer_data;

Widget rotate_90 = (Widget)NULL;
Widget auto_rotate = (Widget)NULL;
//char  print_paper_size[20] = "Letter";  // Displayed in dialog, but not used yet.
int   print_rotated = 0;
int   print_auto_rotation = 0;
//float print_scale = 1.0;                // Not used yet.
int   print_auto_scale = 0;
//int   print_blank_background_color = 0; // Not used yet.
int   print_in_monochrome = 0;
int   print_resolution = 150;           // 72 dpi is normal for Postscript.
// 100 or 150 dpi work well with HP printer
int   print_invert = 0;                 // Reverses black/white

char printer_program[MAX_FILENAME+1];
char previewer_program[MAX_FILENAME+1];

time_t last_snapshot = 0;               // Used to determine when to take next snapshot
time_t last_kmlsnapshot = 0;               // Used to determine when to take next kml snapshot
int doing_snapshot = 0;
int snapshot_interval = 0;


int mag;
int npoints;    /* number of points in a line */


float raster_map_intensity = 0.65;    // Raster map color intensity, set from Maps->Map Intensity
float imagemagick_gamma_adjust = 0.0;  // Additional imagemagick map gamma correction, set from Maps->Adjust Gamma

// Storage for the index file timestamp
time_t map_index_timestamp;

int grid_size = 0;

// Rounding
#ifndef HAVE_ROUNDF
// Poor man's rounding, but rounds away from zero as roundf is supposed to.

float roundf(float x)
{
  int i;
  i= (((x)>=0)?(((x)+.5)):(((x)-.5)));
  return ((float)i);
}

#endif


// UTM Grid stuff
//
//#define UTM_DEBUG
//#define UTM_DEBUG_VERB
//#define UTM_DEBUG_ALLOC
//
static inline int  max_i(int  a, int  b)
{
  return (a > b ? a : b);
}
#define UTM_GRID_EQUATOR 10000000
// the maximum number of UTM zones that will appear on a screen that has a
// high enough resolution to display the within-zone utm grid
#define UTM_GRID_MAX_ZONES      4
// the maximum number of grid lines in each direction shown in each zone
// on the screen at one time
#define UTM_GRID_MAX_COLS_ROWS 64
#define UTM_GRID_DEF_NALLOCED   8
#define UTM_GRID_RC_EMPTY  0xffff

// the hash stores the upper left and lower right boundaries of the
// display of a utm grid to determine whether it matches the current
// screen boundaries or whether the grid needs to be recalculated
// for the current screen.
typedef struct
{
  long ul_x;
  long ul_y;
  long lr_x;
  long lr_y;
} hash_t;

// The utm grid is drawn by connecting a grid of points marking the
// intersection of the easting and northing lines.  col_or_row holds
// this grid of points for the portion of a utm zone visible on the
// screen.  Zone boundaries are not directly represented here.
// In particular, the parallels separating lettered zone rows are
// not directly represented here.
typedef struct
{
  int firstpoint;
  int npoints;
  int nalloced;
  XPoint *points;
} col_or_row_t;

// The portion of a utm zone visible on the screen has some number of
// visible easting lines and northing lines forming a grid that covers
// part or all of the screen.  Zone holds arrays of the coordinates
// of the visible easting and northing intersections, and the next set
// of intersections off screen.  The coarseness of this grid is
// determined by the current scale and stored in utm_grid_spacing_m.
typedef struct
{
  unsigned int ncols;
  col_or_row_t col[UTM_GRID_MAX_COLS_ROWS];
  unsigned int nrows;
  col_or_row_t row[UTM_GRID_MAX_COLS_ROWS];
  int          boundary_x;
} zone_t;

// A utm grid is represented by its hash (upper left and lower right coordinates)
// which are used to check whether or not it fits the current screen or needs to
// be recalculated, and an array of the zones visible on the screen, each containing
// arrays of the points marking the easting/northing intersections of a zone (with
// one or more lettered zone rows).
typedef struct
{
  hash_t       hash;
  unsigned int nzones;
  zone_t       zone[UTM_GRID_MAX_ZONES];
} utm_grid_t;

utm_grid_t utm_grid;

unsigned int utm_grid_spacing_m;
// ^ the above is extern'ed in maps.h for use by draw_ruler_text in
// db.c





// Clear out/set up the UTM/MGRS minor grid intersection arrays.
//
// do_alloc = 0:  Don't allocate memory
//            1:  Allocate memory for the points
//
// Returns: 0 if ok
//          1 if malloc problem
//
int utm_grid_clear(int do_alloc)
{
  int i, j;

  utm_grid.hash.ul_x = 0;
  utm_grid.hash.ul_y = 0;
  utm_grid.hash.lr_x = 0;
  utm_grid.hash.lr_y = 0;
  utm_grid.nzones    = 0;

  for (i=0; i < UTM_GRID_MAX_ZONES; i++)
  {

    utm_grid.zone[i].ncols = utm_grid.zone[i].nrows = 0;
    utm_grid.zone[i].boundary_x = 0;

    for (j=0; j < UTM_GRID_MAX_COLS_ROWS; j++)
    {

      //
      // Clear out column arrays
      //
      utm_grid.zone[i].col[j].firstpoint = UTM_GRID_RC_EMPTY;
      utm_grid.zone[i].col[j].npoints    = 0;

      if (utm_grid.zone[i].col[j].nalloced)
      {
        free(utm_grid.zone[i].col[j].points);
      }

      utm_grid.zone[i].col[j].nalloced   = 0;
      utm_grid.zone[i].col[j].points = NULL;

      if (do_alloc)
      {

        // Allocate enough space for 8 points
        utm_grid.zone[i].col[j].points = calloc(UTM_GRID_DEF_NALLOCED, sizeof(XPoint));
        if (!utm_grid.zone[i].col[j].points)
        {
          fprintf(stderr,"calloc(%d, %d) for z=%d col=%d FAILED!\n",
                  UTM_GRID_DEF_NALLOCED,
                  (int)sizeof(XPoint),
                  i,
                  j);
//                    abort();  // Causes a segfault
          return(1);
        }
        utm_grid.zone[i].col[j].nalloced = UTM_GRID_DEF_NALLOCED;
      }

      //
      // Clear out row arrays
      //
      utm_grid.zone[i].row[j].firstpoint = UTM_GRID_RC_EMPTY;
      utm_grid.zone[i].row[j].npoints    = 0;
      utm_grid.zone[i].row[j].nalloced   = 0;

      if (utm_grid.zone[i].row[j].nalloced)
      {
        free(utm_grid.zone[i].row[j].points);
      }

      utm_grid.zone[i].row[j].nalloced   = 0;
      utm_grid.zone[i].row[j].points = NULL;

      if (do_alloc)
      {

        // Allocate enough space for 8 points
        utm_grid.zone[i].row[j].points = calloc(UTM_GRID_DEF_NALLOCED, sizeof(XPoint));
        if (!utm_grid.zone[i].row[j].points)
        {
          fprintf(stderr,"calloc(%d, %d) for z=%d row=%d FAILED!\n",
                  UTM_GRID_DEF_NALLOCED,
                  (int)sizeof(XPoint),
                  i,
                  j);
//                    abort();  // Causes a segfault
          return(1);
        }
        utm_grid.zone[i].row[j].nalloced = UTM_GRID_DEF_NALLOCED;
      }
    }
  }
  return(0);
}





void maps_init(void)
{
  fprintf(stderr,"\n\nBuilt-in map types:\n");
  fprintf(stderr,"%10s   USGS GNIS Datapoints\n","gnis");
  fprintf(stderr,"%10s   USGS GNIS Datapoints w/population\n","pop");
  fprintf(stderr,"%10s   APRSdos Maps\n","map");
  fprintf(stderr,"%10s   WinAPRS/MacAPRS/X-APRS Maps\n","map");

  fprintf(stderr,"\nSupport for these additional map types has been compiled in: \n");

#ifdef HAVE_MAGICK
  fprintf(stderr,"%10s   Image Map (ImageMagick/GraphicsMagick library, many formats allowed)\n","geo");
#endif  // HAVE_MAGICK

#ifndef NO_GRAPHICS
#ifdef HAVE_LIBCURL
  fprintf(stderr,"%10s   URL (Internet maps via libcurl library)\n","geo");
  fprintf(stderr,"%10s   URL (OpenStreetMaps via libcurl library\n","geo");
  fprintf(stderr,"%10s        Copyright OpenStreetMap and contributors, CC-BY-SA)\n", "");

#else
#ifdef HAVE_WGET
  fprintf(stderr,"%10s   URL (Internet maps via wget)\n","geo");
  fprintf(stderr,"%10s   URL (OpenStreetMaps via wget\n","geo");
  fprintf(stderr,"%10s        Copyright OpenStreetMap and contributors, CC-BY-SA)\n", "");
#endif  // HAVE_WGET
#endif  // HAVE_LIBCURL
#endif  // NO_GRAPHICS


#ifdef HAVE_LIBSHP
  fprintf(stderr,"%10s   ESRI Shapefile Maps (Shapelib library)\n","shp");
#endif  // HAVE_LIBSHP


#ifdef HAVE_LIBGEOTIFF
  fprintf(stderr,"%10s   USGS DRG Geotiff Topographic Maps (libgeotiff/libproj)\n","tif");
#endif  // HAVE_LIBGEOTIFF

#ifndef NO_XPM
  fprintf(stderr,"%10s   X Pixmap Maps (XPM library)\n","xpm");
#endif  // NO_XPM

  init_critical_section( &print_properties_dialog_lock );
  init_critical_section( &print_postscript_dialog_lock );

  // Clear the minor UTM/MGRS grid arrays.  Do _not_ allocate
  // memory for the points.
  (void)utm_grid_clear(0);
}





/*
 *  Calculate NS distance scale at a given location
 *  in meters per Xastir unit
 */
double calc_dscale_y(long UNUSED(x), long UNUSED(y) )
{

  // approximation by looking at +/- 0.5 minutes offset
  //    (void)(calc_distance(y-3000, x, y+3000, x)/6000.0);
  // but this scale is fixed at 1852/6000
  return((double)(1852.0/6000.0));
}





/*
 *  Calculate EW distance scale at a given location
 *  in meters per Xastir unit
 */
double calc_dscale_x(long x, long y)
{

  // approximation by looking at +/- 0.5 minutes offset
  // we should find a better formula...
  return(calc_distance(y, x-3000, y, x+3000)/6000.0);
}





/*
 *  Calculate x map scaling for current location
 *  With that we could have equal distance scaling or a better
 *  view for pixel maps
 */
long get_x_scale(long x, long y, long ysc)
{
  long   xsc;
  double sc_x;
  double sc_y;

  sc_x = calc_dscale_x(x,y);          // meter per Xastir unit
  sc_y = calc_dscale_y(x,y);
  if (sc_x < 0.01 || ysc > 50000)
    // keep it near the poles (>88 deg) or if big parts of world seen
  {
    xsc = ysc;
  }
  else
    // adjust y scale, so that the distance is identical in both directions:
  {
    xsc = (long)(ysc * sc_y / sc_x +0.4999);
  }

  //fprintf(stderr,"Scale: x %5.3fkm/deg, y %5.3fkm/deg, x %ld y %ld\n",sc_x*360,sc_y*360,xsc,ysc);
  return(xsc);
}





/** MAP DRAWING ROUTINES **/



// Function to perform 2D line-clipping Using the improved parametric
// line-clipping algorithm by Liang, Barsky, and Slater published in
// the paper: "Some Improvements to a Parametric Line Clipping
// Algorithm", 1992.  Called by clip2d() function below.  This
// function is set up for float values.  See the clipt_long() and
// clipt_int() functions for use with other types of values.
//
// Returns False if the line is rejected, True otherwise.  May modify
// t0 and t1.
//
int clipt(float p, float q, float *t0, float *t1)
{
  float r;
  int accept = True;


  if (p < 0)    // Entering visible region, so compute t0
  {

    if (q < 0)    // t0 will be non-negative, so continue
    {

      r = q / p;

      if (r > *t1)    // t0 will exceed t1, so reject
      {
        accept = False;
      }
      else if (r > *t0)    // t0 is max of r's
      {
        *t0 = r;
      }
    }
  }
  else
  {

    if (p > 0)    // Exiting visible region, so compute t1
    {

      if (q < p)    // t1 will be <= 1, so continue
      {

        r = q / p;

        if (r < *t0)    // t1 will be <= t0, so reject
        {
          accept = False;
        }
        else if (r < *t1)    // t1 is min of r's
        {
          *t1 = r;
        }
      }
    }
    else    // p == 0
    {

      if (q < 0)    // Line parallel and outside, so reject
      {
        accept = False;
      }
    }
  }
  return(accept);
}





// Function to perform 2D line-clipping using the improved parametric
// line-clipping algorithm by Liang, Barsky, and Slater published in
// the paper: "Some Improvements to a Parametric Line Clipping
// Algorithm", 1992.  Uses the clipt() function above.
//
// Returns False if the line is rejected, True otherwise.  x0, y0,
// x1, y1 may get modified by this function.  These will be the new
// clipped line ends that fit inside the window.
//
// Clip 2D line segment with endpoints (x0,y0) and (x1,y1).  The clip
// window is x_left <= x <= x_right and y_bottom <= y <= y_top.
//
// This function is set up for float values.  See the clip2d_long()
// and clip2d_screen() functions for use with other types of values.
//
int clip2d(float *x0, float *y0, float *x1, float *y1)
{
  float t0, t1;
  float delta_x, delta_y;
  int visible = False;
  float x_left, y_top;        // NW corner of screen
  float x_right, y_bottom;    // SE corner of screen


  // Assign floating point values for screen corners
  y_top = f_NW_corner_latitude;
  y_bottom = f_SE_corner_latitude;
  x_left = f_NW_corner_longitude;
  x_right =  f_SE_corner_longitude;

  if (       ((*x0 < x_left)   && (*x1 < x_left))
             || ((*x0 > x_right)  && (*x1 > x_right))
             || ((*y0 < y_bottom) && (*y1 < y_bottom))
             || ((*y0 > y_top)    && (*y1 > y_top)) )
  {

    // Both endpoints are on outside and same side of visible
    // region, so reject.
    return(visible);
  }

  t0 = 0;
  t1 = 1;
  delta_x = *x1 - *x0;

  if ( clipt(-delta_x, *x0 - x_left, &t0, &t1) )             // left
  {

    if ( clipt(delta_x, x_right - *x0, &t0, &t1) )         // right
    {

      delta_y = *y1 - *y0;

      if ( clipt(-delta_y, *y0 - y_bottom, &t0, &t1) )   // bottom
      {

        if ( clipt(delta_y, y_top - *y0, &t0, &t1) )   // top
        {
          // Compute coordinates

          if (t1 < 1)     // Compute V1' (new x1,y1)
          {
            *x1 = *x0 + t1 * delta_x;
            *y1 = *y0 + t1 * delta_y;
          }

          if (t0 > 0)     // Compute V0' (new x0,y0)
          {
            *x0 = *x0 + t0 * delta_x;
            *y0 = *y0 + t0 * delta_y;
          }
          visible = True;
        }
      }
    }
  }
  return(visible);
}





// Function to perform 2D line-clipping Using the improved parametric
// line-clipping algorithm by Liang, Barsky, and Slater published in
// the paper: "Some Improvements to a Parametric Line Clipping
// Algorithm", 1992.  Called by clip2d_long() function below.  This
// function is set up for Xastir coordinate values (unsigned longs).
// See the clipt() and clipt_int() functions for use with other
// types of values.
//
// Returns False if the line is rejected, True otherwise.  May modify
// t0 and t1.
//
int clipt_long(long p, long q, float *t0, float *t1)
{
  float r;
  int accept = True;


  if (p < 0)    // Entering visible region, so compute t0
  {

    if (q < 0)    // t0 will be non-negative, so continue
    {

      r = q / (p * 1.0);

      if (r > *t1)    // t0 will exceed t1, so reject
      {
        accept = False;
      }
      else if (r > *t0)    // t0 is max of r's
      {
        *t0 = r;
      }
    }
  }
  else
  {

    if (p > 0)    // Exiting visible region, so compute t1
    {

      if (q < p)    // t1 will be <= 1, so continue
      {

        r = q / (p * 1.0);

        if (r < *t0)    // t1 will be <= t0, so reject
        {
          accept = False;
        }
        else if (r < *t1)    // t1 is min of r's
        {
          *t1 = r;
        }
      }
    }
    else    // p == 0
    {

      if (q < 0)    // Line parallel and outside, so reject
      {
        accept = False;
      }
    }
  }
  //fprintf(stderr,"clipt_long: %d\n",accept);
  return(accept);
}





// Function to perform 2D line-clipping using the improved parametric
// line-clipping algorithm by Liang, Barsky, and Slater published in
// the paper: "Some Improvements to a Parametric Line Clipping
// Algorithm", 1992.  Uses the clipt_long() function above.
//
// Returns False if the line is rejected, True otherwise.  x0, y0,
// x1, y1 may get modified by this function.  These will be the new
// clipped line ends that fit inside the window.
//
// Clip 2D line segment with endpoints (x0,y0) and (x1,y1).  The clip
// window is x_left <= x <= x_right and y_bottom <= y <= y_top.
//
// This function uses the Xastir coordinate system.  We had to flip
// y_bottom/y_top below due to the coordinate system being
// upside-down.
//
// This function is set up for Xastir coordinate values (unsigned
// longs).  See the clip2d() or clip2d_screen() functions for use
// with other types of values.
//
int clip2d_long(unsigned long *x0, unsigned long *y0, unsigned long *x1, unsigned long *y1)
{
  float t0, t1;
  long delta_x, delta_y;
  int visible = False;
  unsigned long x_left   = NW_corner_longitude;
  unsigned long x_right = SE_corner_longitude;
  // Reverse the following two as our Xastir coordinate system is
  // upside down.  The algorithm requires this order.
  unsigned long y_top = SE_corner_latitude;
  unsigned long y_bottom = NW_corner_latitude;


  if (       ( (*x0 < x_left  ) && (*x1 < x_left  ) )
             || ( (*x0 > x_right ) && (*x1 > x_right ) )
             || ( (*y0 < y_bottom) && (*y1 < y_bottom) )
             || ( (*y0 > y_top   ) && (*y1 > y_top   ) ) )
  {

    // Both endpoints are on same side of visible region and
    // outside of it, so reject.
    //fprintf(stderr,"reject 1\n");
    return(visible);
  }

  t0 = 0;
  t1 = 1;
  delta_x = *x1 - *x0;

  if ( clipt_long(-delta_x, *x0 - x_left, &t0, &t1) )             // left
  {

    if ( clipt_long(delta_x, x_right - *x0, &t0, &t1) )         // right
    {

      delta_y = *y1 - *y0;

      if ( clipt_long(-delta_y, *y0 - y_bottom, &t0, &t1) )   // bottom
      {

        if ( clipt_long(delta_y, y_top - *y0, &t0, &t1) )   // top
        {
          // Compute coordinates

          if (t1 < 1)     // Compute V1' (new x1,y1)
          {
            *x1 = *x0 + t1 * delta_x;
            *y1 = *y0 + t1 * delta_y;
          }

          if (t0 > 0)     // Compute V0' (new x0,y0)
          {
            *x0 = *x0 + t0 * delta_x;
            *y0 = *y0 + t0 * delta_y;
          }
          visible = True;
        }
        else
        {
          //fprintf(stderr,"reject top\n");
        }
      }
      else
      {
        //fprintf(stderr,"reject bottom\n");
      }
    }
    else
    {
      //fprintf(stderr,"reject right\n");
    }
  }
  else
  {
    //fprintf(stderr,"reject left\n");
  }
  return(visible);
}





// Function to perform 2D line-clipping Using the improved parametric
// line-clipping algorithm by Liang, Barsky, and Slater published in
// the paper: "Some Improvements to a Parametric Line Clipping
// Algorithm", 1992.  Called by clip2d_screen() function below.  This
// function is set up for screen coordinate values (unsigned ints).
// See the clipt() and clipt_long functions for use with other types
// of values.
//
// Returns False if the line is rejected, True otherwise.  May modify
// t0 and t1.
//
int clipt_int(int p, int q, float *t0, float *t1)
{
  float r;
  int accept = True;


  if (p < 0)    // Entering visible region, so compute t0
  {

    if (q < 0)    // t0 will be non-negative, so continue
    {

      r = q / (p * 1.0);

      if (r > *t1)    // t0 will exceed t1, so reject
      {
        accept = False;
      }
      else if (r > *t0)    // t0 is max of r's
      {
        *t0 = r;
      }
    }
  }
  else
  {

    if (p > 0)    // Exiting visible region, so compute t1
    {

      if (q < p)    // t1 will be <= 1, so continue
      {

        r = q / (p * 1.0);

        if (r < *t0)    // t1 will be <= t0, so reject
        {
          accept = False;
        }
        else if (r < *t1)    // t1 is min of r's
        {
          *t1 = r;
        }
      }
    }
    else    // p == 0
    {

      if (q < 0)    // Line parallel and outside, so reject
      {
        accept = False;
      }
    }
  }
  //fprintf(stderr,"clipt_int: %d\n",accept);
  return(accept);
}





// Function to perform 2D line-clipping using the improved parametric
// line-clipping algorithm by Liang, Barsky, and Slater published in
// the paper: "Some Improvements to a Parametric Line Clipping
// Algorithm", 1992.  Uses the clipt_int() function above.
//
// Returns False if the line is rejected, True otherwise.  x0, y0,
// x1, y1 may get modified by this function.  These will be the new
// clipped line ends that fit inside the window.
//
// Clip 2D line segment with endpoints (x0,y0) and (x1,y1).  The clip
// window is x_left <= x <= x_right and y_bottom <= y <= y_top.
//
// This function uses the screen coordinate system.  We had to flip
// y_bottom/y_top below due to the coordinate system being
// upside-down.
//
// This function is set up for screen coordinate values (unsigned
// ints).  See the clip2d() and clip2d_long() functions for use
// with other types of values.
//
int clip2d_screen(unsigned int *x0, unsigned int *y0, unsigned int *x1, unsigned int *y1)
{
  float t0, t1;
  int delta_x, delta_y;
  int visible = False;
  unsigned int x_right  = screen_width;
  unsigned int x_left   = 0;
  // Reverse the following two as our screen coordinate system is
  // upside down.  The algorithm requires this order.
  unsigned int y_top    = screen_height;
  unsigned int y_bottom = 0;


  if (       ( (*x0 < x_left  ) && (*x1 < x_left  ) )
             || ( (*x0 > x_right ) && (*x1 > x_right ) )
             || ( (*y0 < y_bottom) && (*y1 < y_bottom) )
             || ( (*y0 > y_top   ) && (*y1 > y_top   ) ) )
  {

    // Both endpoints are on same side of visible region and
    // outside of it, so reject.
    //fprintf(stderr,"reject 1\n");
    return(visible);
  }

  t0 = 0;
  t1 = 1;
  delta_x = *x1 - *x0;

  if ( clipt_int(-delta_x, *x0 - x_left, &t0, &t1) )             // left
  {

    if ( clipt_int(delta_x, x_right - *x0, &t0, &t1) )         // right
    {

      delta_y = *y1 - *y0;

      if ( clipt_int(-delta_y, *y0 - y_bottom, &t0, &t1) )   // bottom
      {

        if ( clipt_int(delta_y, y_top - *y0, &t0, &t1) )   // top
        {
          // Compute coordinates

          if (t1 < 1)     // Compute V1' (new x1,y1)
          {
            *x1 = *x0 + t1 * delta_x;
            *y1 = *y0 + t1 * delta_y;
          }

          if (t0 > 0)     // Compute V0' (new x0,y0)
          {
            *x0 = *x0 + t0 * delta_x;
            *y0 = *y0 + t0 * delta_y;
          }
          visible = True;
        }
        else
        {
          //fprintf(stderr,"reject top\n");
        }
      }
      else
      {
        //fprintf(stderr,"reject bottom\n");
      }
    }
    else
    {
      //fprintf(stderr,"reject right\n");
    }
  }
  else
  {
    //fprintf(stderr,"reject left\n");
  }
  return(visible);
}





// Draws a point onto a pixmap.  Assumes that the proper GC has
// already been set up for drawing the correct color/width/linetype,
// etc.  If the bounding box containing the point doesn't intersect
// with the current view, the point isn't drawn.
//
// Input point is in the Xastir coordinate system.
//
void draw_point(Widget w,
                unsigned long x1,
                unsigned long y1,
                GC gc,
                Pixmap which_pixmap,
                int skip_duplicates)
{

  int x1i, y1i;
  static int last_x1i;
  static int last_y1i;


  // Check whether the two bounding boxes intersect.  If not, skip
  // the rest of this function.  Do we need to worry about
  // special-case code here to handle vertical/horizontal lines
  // (width or length of zero)?
  //
//
// WE7U
// Check to see if we can do this faster than map_visible() can.  A
// point is a special case that should take only four compares
// against the map window boundaries.
//
  if (!map_visible(y1, y1, x1, x1))
  {
    // Skip this vector
    //fprintf(stderr,"Point not visible\n");
    return;
  }

  // Convert to screen coordinates.  Careful here!
  // The format conversions you'll need if you try to
  // compress this into two lines will get you into
  // trouble.
  x1i = x1 - NW_corner_longitude;
  x1i = x1i / scale_x;

  y1i = y1 - NW_corner_latitude;
  y1i = y1i / scale_y;

  if (skip_duplicates)
  {
    if (x1i == last_x1i && y1i == last_y1i)
    {
      return;
    }
  }

  // XDrawPoint uses 16-bit unsigned integers
  // (shorts).  Make sure we stay within the limits.
  (void)XDrawPoint(XtDisplay(w),
                   which_pixmap,
                   gc,
                   l16(x1i),
                   l16(y1i));

  last_x1i = x1i;
  last_y1i = y1i;
}





// Draws a vector onto a pixmap.  Assumes that the proper GC has
// already been set up for drawing the correct color/width/linetype,
// etc.
//
// Input points are in lat/long coordinates.
//
void draw_point_ll(Widget w,
                   float y1,   // lat1
                   float x1,   // long1
                   GC gc,
                   Pixmap which_pixmap,
                   int skip_duplicates)
{

  unsigned long x1L, y1L;

//
// WE7U
// We should probably do four floating point compares against the
// map boundaries here in order to speed up rejection of points that
// don't fit our window.  If we change to that, copy the conversion-
// to-screen-coordinates and drawing stuff from draw_point() down to
// this routine so that we don't go through the comparisons again
// there.
//
  // Convert the point to the Xastir coordinate system.
  convert_to_xastir_coordinates(&x1L,
                                &y1L,
                                x1,
                                y1);

  // Call the draw routine above.
  draw_point(w, x1L, y1L, gc, which_pixmap, skip_duplicates);
}





// Draws a vector onto a pixmap.  Assumes that the proper GC has
// already been set up for drawing the correct color/width/linetype,
// etc.  If the bounding box containing the vector doesn't intersect
// with the current view, the line isn't drawn.
//
// Input points are in the Xastir coordinate system.
//
void draw_vector(Widget w,
                 unsigned long x1,
                 unsigned long y1,
                 unsigned long x2,
                 unsigned long y2,
                 GC gc,
                 Pixmap which_pixmap,
                 int skip_duplicates)
{

  int x1i, x2i, y1i, y2i;
  static int last_x1i, last_x2i, last_y1i, last_y2i;


  //fprintf(stderr,"%ld,%ld  %ld,%ld\t",x1,y1,x2,y2);
  if ( !clip2d_long(&x1, &y1, &x2, &y2) )
  {
    // Skip this vector
    //fprintf(stderr,"Line not visible\n");
    //fprintf(stderr,"%ld,%ld  %ld,%ld\n",x1,y1,x2,y2);
    return;
  }
  //fprintf(stderr,"%ld,%ld  %ld,%ld\n",x1,y1,x2,y2);

  // Convert to screen coordinates.  Careful here!
  // The format conversions you'll need if you try to
  // compress this into two lines will get you into
  // trouble.
  x1i = x1 - NW_corner_longitude;
  x1i = x1i / scale_x;

  y1i = y1 - NW_corner_latitude;
  y1i = y1i / scale_y;

  x2i = x2 - NW_corner_longitude;
  x2i = x2i / scale_x;

  y2i = y2 - NW_corner_latitude;
  y2i = y2i / scale_y;

  if (skip_duplicates)
  {
    if (last_x1i == x1i
        && last_x2i == x2i
        && last_y1i == y1i
        && last_y2i == y2i)
    {
      return;
    }
  }

  // XDrawLine uses 16-bit unsigned integers
  // (shorts).  Make sure we stay within the limits.
  // clip2d_long() should make sure of this anyway as it clips
  // lines to fit the window.
  //
  (void)XDrawLine(XtDisplay(w),
                  which_pixmap,
                  gc,
                  l16(x1i),
                  l16(y1i),
                  l16(x2i),
                  l16(y2i));

  last_x1i = x1i;
  last_x2i = x2i;
  last_y1i = y1i;
  last_y2i = y2i;
}





// Draws a vector onto a pixmap.  Assumes that the proper GC has
// already been set up for drawing the correct color/width/linetype,
// etc.
//
// Input points are in lat/long coordinates.
//
void draw_vector_ll(Widget w,
                    float y1,   // lat1
                    float x1,   // long1
                    float y2,   // lat2
                    float x2,   // long2
                    GC gc,
                    Pixmap which_pixmap,
                    int skip_duplicates)
{

  unsigned long x1L, x2L, y1L, y2L;
  int x1i, x2i, y1i, y2i;
  static int last_x1i, last_x2i, last_y1i, last_y2i;


  //fprintf(stderr,"%lf,%lf  %lf,%lf\t",x1,y1,x2,y2);
  if ( !clip2d(&x1, &y1, &x2, &y2) )
  {
    // Skip this vector
//fprintf(stderr,"Line not visible: %lf,%lf  %lf,%lf\n",y1,x1,y2,x2);
    return;
  }
//fprintf(stderr,"%lf,%lf  %lf,%lf\n",x1,y1,x2,y2);

  // Convert the points to the Xastir coordinate system.
  convert_to_xastir_coordinates(&x1L,
                                &y1L,
                                x1,
                                y1);

  convert_to_xastir_coordinates(&x2L,
                                &y2L,
                                x2,
                                y2);

//fprintf(stderr,"%ld,%ld  %ld,%ld\n",x1L,y1L,x2L,y2L);

  // Convert to screen coordinates.  Careful here!
  // The format conversions you'll need if you try to
  // compress this into two lines will get you into
  // trouble.
  x1i = (int)(x1L - NW_corner_longitude);
  x1i = x1i / scale_x;

  y1i = (int)(y1L - NW_corner_latitude);
  y1i = y1i / scale_y;

  x2i = (int)(x2L - NW_corner_longitude);
  x2i = x2i / scale_x;

  y2i = (int)(y2L - NW_corner_latitude);
  y2i = y2i / scale_y;

  if (skip_duplicates)
  {
    if (last_x1i == x1i
        && last_x2i == x2i
        && last_y1i == y1i
        && last_y2i == y2i)
    {
//            fprintf(stderr,"Duplicate line\n");
      return;
    }
  }
//fprintf(stderr,"NW_corner_latitude:%ld, NW_corner_longitude:%ld, scale_x:%ld, scale_y:%ld\n",
//    NW_corner_latitude,
//    NW_corner_longitude,
//    scale_x,
//    scale_y);

//fprintf(stderr,"%d,%d  %d,%d\n\n",x1i,y1i,x2i,y2i);

  // XDrawLine uses 16-bit unsigned integers
  // (shorts).  Make sure we stay within the limits.
  // clip2d() should make sure of this anyway as it clips lines to
  // fit the window.
  //
  (void)XDrawLine(XtDisplay(w),
                  which_pixmap,
                  gc,
                  l16(x1i),
                  l16(y1i),
                  l16(x2i),
                  l16(y2i));

  last_x1i = x1i;
  last_x2i = x2i;
  last_y1i = y1i;
  last_y2i = y2i;
}





// Find length of a standard string of seven zeroes in the border font.
// Supporting function for draw_grid() and the grid drawing functions.
int get_standard_border_string_width_pixels(Widget w, int length)
{
  int string_width_pixels = 0; // Width of the unrotated seven_zeroes label string in pixels.
  char seven_zeroes[8] = "0000000";
  char five_zeroes[6]  = "00000";

  if (length==5)
  {
    string_width_pixels = get_rotated_label_text_length_pixels(w, five_zeroes, FONT_BORDER);
  }

  string_width_pixels = get_rotated_label_text_length_pixels(w, seven_zeroes, FONT_BORDER);
  return string_width_pixels;
}





// Find height of a standard string in the border font
// Supporting function for draw_grid() and the grid drawing functions.
int get_standard_border_string_height_pixels(Widget w)
{
  int string_width_pixels = 0; // Width of the unrotated seven_zeroes label string in pixels.
  char one_zero[2] = "0";

  string_width_pixels = get_rotated_label_text_height_pixels(w, one_zero, FONT_BORDER);
  return string_width_pixels;
}





// Find out the width to use for the border to apply when uing a labeled grid.
// Border width is determined from the height of the border font.
// Supporting function for draw_grid() and the grid drawing functions.
int get_border_width(Widget w)
{
  int border_width = 14;      // The width of the border to draw around the
  // map to place labeled tick marks into
  // should be an even number.
  // The default here is overidden by size of the border font.
  int string_height_pixels = 0; // Height of a string in the border font in pixels

  string_height_pixels = get_standard_border_string_height_pixels(w);
  // Rotated text functions used to draw the border text add some
  // blank space at the bottom of the text so make the border wide enough
  // to compensate for this.
  border_width = string_height_pixels + (string_height_pixels/2) + 1;
  // check to see if string_height_pixels is even
  if ((float)string_height_pixels/2.0!=floor((float)string_height_pixels/2.0))
  {
    border_width++;
  }
  // we are using draw nice string to write the metadata in the top border, so
  // make sure the border is wide enough for it, even if the grid labels are small.
  if (border_width < 14)
  {
    border_width = 14;
  }
  return border_width;
}





// ***********************************************************
//
// get_horizontal_datum()
//
// Provides the current map datum.  Current default is WGS84.
// Parameters: datum, character array ponter for the string
//             that will be filled with the current datum
// sizeof_datum, the size of the datum character array.
//
//***********************************************************
void get_horizontal_datum(char *datum, int sizeof_datum)
{
  char metadata_datum[6] = "WGS84";  // datum to display in metadata on top border
  xastir_snprintf(datum, sizeof_datum, "%s", metadata_datum);
  if (sizeof_datum<6)
  {
    fprintf(stderr,"Datum [%s] truncated to [%s]\n",metadata_datum,datum);
  }
}





// Lat/Long coordinate system, draw lat/long lines.  Called by
// draw_grid() below.
//
void draw_complete_lat_lon_grid(Widget w)
{

  int coord;
  char dash[2];
  unsigned int x,x1,x2;
  unsigned int y,y1,y2;
  unsigned int stepsx;         // spacing of grid lines
  unsigned int stepsy;         // spacing of grid lines
  int coordinate_format;      // Format to use for coordinates on border (e.g. decimal degrees).
  char grid_label[25];        // String to draw labels on grid lines
  int screen_width_xastir;  // screen width in xastir units (1/100 of a second)
//  int screen_height_xastir; // screen height in xastir units (1/100 of a second)
  int border_width;          // the width of the labeled border in pixels.
  int string_width_pixels = 0;// Width of a grid label string in pixels.
  float screen_width_degrees;   // Width of the screen in degrees
  int log_screen_width_degrees; // Log10 of the screen width in degrees, used to scale degrees
  long xx2, yy2;
  long xx, yy;
  unsigned int last_label_end;  // cordinate of the end of the previous label
  char metadata_datum[6];  // datum to display in metadata on top border
  char grid_label1[25];       // String to draw latlong metadata
  char grid_label2[25];       // String to draw latlong metadata
  char top_label[180];        // String to draw metadata on top border

  if (!long_lat_grid) // We don't wish to draw a map grid
  {
    return;
  }

  // convert between selected coordinate format constant and display format constants
  if (coordinate_system == USE_DDDDDD)
  {
    coordinate_format = CONVERT_DEC_DEG;
  }
  else if (coordinate_system == USE_DDMMSS)
  {
    coordinate_format = CONVERT_DMS_NORMAL_FORMATED;
  }
  else
  {
    coordinate_format = CONVERT_HP_NORMAL_FORMATED;
  }
  border_width = get_border_width(w);
  // Find xastir coordinates of upper left and lower right corners.
  xx = NW_corner_longitude  + (border_width * scale_x);
  yy = NW_corner_latitude   + (border_width * scale_y);
  xx2 = NW_corner_longitude  + ((screen_width - border_width) * scale_x);
  yy2 = NW_corner_latitude   + ((screen_height - border_width) * scale_y);
  screen_width_xastir = xx2 - xx;
//  screen_height_xastir = yy2 - yy;
  // Determine some parameters used in drawing the border.
  string_width_pixels = get_standard_border_string_width_pixels(w, 7);
  // 1 xastir coordinate = 1/100 of a second
  // 100*60*60 xastir coordinates (=360000 xastir coordinates) = 1 degree
  // 64800000 xastir coordinates = 180 degrees
  // 360000   xastir coordinates = 1 degree
  // scale_x * (screen_width/10) = one tenth of the screen width in xastir coordinates
  // scale_x number of xastir coordinates per pixel
  screen_width_degrees = (float)(screen_width_xastir / (float)360000);
  log_screen_width_degrees = log10(screen_width_degrees);


  if (draw_labeled_grid_border==TRUE)
  {
    get_horizontal_datum(metadata_datum, sizeof(metadata_datum));

    // Put metadata in top border.
    // find location of upper left corner of map, convert to Lat/Long
    convert_lon_l2s(xx, grid_label1, sizeof(grid_label1), coordinate_format);
    convert_lat_l2s(yy, grid_label2, sizeof(grid_label2), coordinate_format);

    strcpy(grid_label, grid_label1);
    grid_label[sizeof(grid_label)-1] = '\0';  // Terminate string
    strcat(grid_label, " ");
    grid_label[sizeof(grid_label)-1] = '\0';  // Terminate string
    strcat(grid_label, grid_label2);
    grid_label[sizeof(grid_label)-1] = '\0';  // Terminate string

    // find location of lower right corner of map, convert to Lat/Long
    convert_lon_l2s(xx2, grid_label1, sizeof(grid_label1), coordinate_format);
    convert_lat_l2s(yy2, grid_label2, sizeof(grid_label2), coordinate_format);
    //"XASTIR Map of %s (upper left) to %s %s (lower right).  Lat/Long grid, %s datum. ",
    xastir_snprintf(top_label,
                    sizeof(top_label),
                    langcode("MDATA002"),
                    grid_label,grid_label1,grid_label2,metadata_datum);
    draw_rotated_label_text_to_target (w, 270,
                                       border_width+2,
                                       border_width-1,
                                       sizeof(top_label),colors[0x10],top_label,FONT_BORDER,
                                       pixmap_final,
                                       outline_border_labels, colors[outline_border_labels_color]);
  }

  // A crude grid spacing can be obtained from scaling one tenth of the screen width.
  // This works, but puts the grid lines at arbitrary increments of a degree.
  //stepsx = (scale_x*(screen_width/10));

  // Setting the grid using the base 10 log of the screen width in degrees allows
  // both scaling the grid to the screen and spacing the grid lines at appropriately
  // rounded increments of a degree.
  //
  // Set default grid to 0.1 degree.  This will be used when the screen width is about 1 degree.
  stepsx = 36000;  // if (log_screen_width_degrees == 0)
  // Work out an appropriate grid spacing for the screen size and coordinate system.
  if (log_screen_width_degrees > 0)
  {
    // grid spacing is rounded to 10 degree increment.
    stepsx = ((int)(screen_width_degrees / (pow(10,log_screen_width_degrees)))*pow(10,log_screen_width_degrees)) * 36000;
  }
  if (log_screen_width_degrees < 0)
  {
    // Grid spacing is rounded to less than one degree.
    if (coordinate_system == USE_DDDDDD)
    {
      // Round to tenths or hundrethds or thousanths of a degree.
      stepsx = ((float)(int)(((float)screen_width_degrees / pow(10,log_screen_width_degrees)*10.0)))*pow(10,log_screen_width_degrees) * 3600;
    }
    else
    {
      // For decimal minutes or minutes and seconds.
      // Find screen width and log screen width in minutes.
      screen_width_degrees = screen_width_degrees * 60.0;
      log_screen_width_degrees = log10(screen_width_degrees);
      // round to minutes or tenths of minutes.
      stepsx = ((float)(int)
                ((float)(screen_width_degrees) / pow(10,log_screen_width_degrees) * 10.0)
               )
               * pow(10,log_screen_width_degrees) * 3600;
      if (log_screen_width_degrees==0)
      {
        stepsx = 600; // 6000 = 1 minute
      }
    }
  }
  // Grid should now be close to reasonable spacing for screen size, but
  // may need to be tuned.
  // Test for too tightly or too coarsely spaced grid.
  if (stepsx<(unsigned int)(scale_x * string_width_pixels * 1.5))
  {
    stepsx = stepsx * 2.0;
  }
  if (stepsx<(unsigned int)(scale_x * string_width_pixels * 1.5))
  {
    stepsx = stepsx * 2.0;
  }
  if (stepsx>(unsigned int)((scale_x * screen_width)/3.5))
  {
    stepsx = stepsx / 2.0;
  }
  // Handle special case of very small screen - only draw a single grid line
  if (screen_width < (string_width_pixels * 2))
  {
    stepsx = (scale_x*(screen_width/2));
  }
  // Make sure we don't pass an erronous stepsx of 0 on.
  if (stepsx==0)
  {
    stepsx=36000;
  }

  // Use the same grid spacing for both latitude and longitude grids.
  // We could use a scaling factor related to the screen height width ratio here.
  stepsy = stepsx;

  // protect against division by zero
  if (scale_x==0)
  {
    scale_x = 1;
  }
  if (scale_y==0)
  {
    scale_y = 1;
  }

  // Now draw and label the grid.
  // Draw vertical longitude lines
  if (NW_corner_latitude >= 0)
  {
    y1 = 0;
  }
  else
  {
    y1 = -NW_corner_latitude/scale_y;
  }

  y2 = (180*60*60*100-NW_corner_latitude)/scale_y;

  if (y2 > (unsigned int)screen_height)
  {
    y2 = screen_height-1;
  }

  coord = NW_corner_longitude+stepsx-(NW_corner_longitude%stepsx);
  if (coord < 0)
  {
    coord = 0;
  }

  last_label_end = 0;
  for (; coord < SE_corner_longitude && coord <= 360*60*60*100; coord += stepsx)
  {

    x = (coord-NW_corner_longitude)/scale_x;

    if ((coord%(648000*100)) == 0)
    {
      (void)XSetLineAttributes (XtDisplay (w),
                                gc_tint,
                                1,
                                LineSolid,
                                CapButt,
                                JoinMiter);
      (void)XDrawLine (XtDisplay (w),
                       pixmap_final,
                       gc_tint,
                       l16(x),
                       l16(y1),
                       l16(x),
                       l16(y2));
      (void)XSetLineAttributes (XtDisplay (w),
                                gc_tint,
                                1,
                                LineOnOffDash,
                                CapButt,
                                JoinMiter);
      continue;   // Go to next iteration of for loop
    }
    else if ((coord%(72000*100)) == 0)
    {
      dash[0] = dash[1] = 8;
      (void)XSetDashes (XtDisplay (w), gc_tint, 0, dash, 2);
    }
    else if ((coord%(7200*100)) == 0)
    {
      dash[0] = dash[1] = 4;
      (void)XSetDashes (XtDisplay (w), gc_tint, 0, dash, 2);
    }
    else if ((coord%(300*100)) == 0)
    {
      dash[0] = dash[1] = 2;
      (void)XSetDashes (XtDisplay (w), gc_tint, 0, dash, 2);
    }

    (void)XDrawLine (XtDisplay (w),
                     pixmap_final,
                     gc_tint,
                     l16(x),
                     l16(y1),
                     l16(x),
                     l16(y2));

    if (draw_labeled_grid_border==TRUE)
    {
      // Label the longitudes in lower border.
      convert_lon_l2s(coord, grid_label, sizeof(grid_label), coordinate_format);
      if (log_screen_width_degrees > 0 && strlen(grid_label) > 5)
      {
        // truncate the grid_label string
        if (coordinate_system==USE_DDMMMM)
        {
          // Add ' and move EW and null characters forward.
          grid_label[strlen(grid_label)-5] = '\'';
          grid_label[strlen(grid_label)-4] = grid_label[strlen(grid_label)-1];
          grid_label[strlen(grid_label)-3] = grid_label[strlen(grid_label)];
        }
        else
        {
          // Move EW and null characters forward.
          grid_label[strlen(grid_label)-5] = grid_label[strlen(grid_label)-1];
          grid_label[strlen(grid_label)-4] = grid_label[strlen(grid_label)];
        }
      }
      string_width_pixels = get_rotated_label_text_length_pixels(w, grid_label, FONT_BORDER);
      // test for overlap of label with previously printed label
      if (x > last_label_end + 3 && x < (unsigned int)(screen_width - string_width_pixels))
      {
        draw_rotated_label_text_to_target (w, 270,
                                           x,
                                           screen_height,
                                           sizeof(grid_label),colors[0x09],grid_label,FONT_BORDER,
                                           pixmap_final,
                                           outline_border_labels, colors[outline_border_labels_color]);
        last_label_end = x + string_width_pixels;
      }
    }
  }

  // Draw horizontal latitude lines.
  last_label_end = 0;
  if (NW_corner_longitude >= 0)
  {
    x1 = 0;
  }
  else
  {
    x1 = -NW_corner_longitude/scale_x;
  }

  x2 = (360*60*60*100-NW_corner_longitude)/scale_x;
  if (x2 > (unsigned int)screen_width)
  {
    x2 = screen_width-1;
  }

  coord = NW_corner_latitude+stepsy-(NW_corner_latitude%stepsy);
  if (coord < 0)
  {
    coord = 0;
  }

  for (; coord < SE_corner_latitude && coord <= 180*60*60*100; coord += stepsy)
  {

    y = (coord-NW_corner_latitude)/scale_y;

    if ((coord%(324000*100)) == 0)
    {
      (void)XSetLineAttributes (XtDisplay (w),
                                gc_tint,
                                1,
                                LineSolid,
                                CapButt,
                                JoinMiter);
      (void)XDrawLine (XtDisplay (w),
                       pixmap_final,
                       gc_tint,
                       l16(x1),
                       l16(y),
                       l16(x2),
                       l16(y));
      (void)XSetLineAttributes (XtDisplay (w),
                                gc_tint,
                                1,
                                LineOnOffDash,
                                CapButt,
                                JoinMiter);
      continue;   // Go to next iteration of for loop
    }
    else if ((coord%(36000*100)) == 0)
    {
      dash[0] = dash[1] = 8;
      (void)XSetDashes (XtDisplay (w), gc_tint, 4, dash, 2);
    }
    else if ((coord%(3600*100)) == 0)
    {
      dash[0] = dash[1] = 4;
      (void)XSetDashes (XtDisplay (w), gc_tint, 2, dash, 2);
    }
    else if ((coord%(150*100)) == 0)
    {
      dash[0] = dash[1] = 2;
      (void)XSetDashes (XtDisplay (w), gc_tint, 1, dash, 2);
    }

    (void)XDrawLine (XtDisplay (w),
                     pixmap_final,
                     gc_tint,
                     l16(x1),
                     l16(y),
                     l16(x2),
                     l16(y));

    if (draw_labeled_grid_border==TRUE)
    {
      // label the latitudes on left and right borders
      // (unlike UTM where easting before northing order is important)
      convert_lat_l2s(coord, grid_label, sizeof(grid_label), coordinate_format);
      if (log_screen_width_degrees > 0 && strlen(grid_label) > 5)
      {
        // truncate the grid_label string
        if (coordinate_system==USE_DDMMMM)
        {
          // Add ' and move EW and null characters forward.
          grid_label[strlen(grid_label)-5] = '\'';
          grid_label[strlen(grid_label)-4] = grid_label[strlen(grid_label)-1];
          grid_label[strlen(grid_label)-3] = grid_label[strlen(grid_label)];
        }
        else
        {
          // Move EW and null characters forward.
          grid_label[strlen(grid_label)-5] = grid_label[strlen(grid_label)-1];
          grid_label[strlen(grid_label)-4] = grid_label[strlen(grid_label)];
        }
      }
      string_width_pixels = get_rotated_label_text_length_pixels(w, grid_label, FONT_BORDER);
      // check to make sure we aren't overwriting the previous label text
      if ((y > last_label_end+3) && (y > (unsigned int)string_width_pixels))
      {
        draw_rotated_label_text_to_target (w, 180,
                                           screen_width,
                                           y,
                                           sizeof(grid_label),colors[0x09],grid_label,FONT_BORDER,
                                           pixmap_final,
                                           outline_border_labels, colors[outline_border_labels_color]);
        draw_rotated_label_text_to_target (w, 180,
                                           border_width,
                                           y,
                                           sizeof(grid_label),colors[0x09],grid_label,FONT_BORDER,
                                           pixmap_final,
                                           outline_border_labels, colors[outline_border_labels_color]);
        last_label_end = y + string_width_pixels;
      }
    }
  }
} // End of draw_complete_lat_lon_grid()





// Draw the major zones for UTM and MGRS.  Called by draw_grid()
// below.
//
// These are based off 6-degree lat/long lines, with a few irregular
// zones that have to be special-cased.  This part of the code
// handles the irregular zones in SW Norway (31V/32V) and the
// regions near Svalbard (31X/33X/35X/37X) just fine.

// These are based off the central meridian running up the middle of
// each zone (3 degrees from either side of the standard six-degree
// zones).  Even the irregular zones key off the same medians.  UTM
// grids are defined in terms of meters instead of lat/long, so they
// don't line up with the left/right edges of the zones or with the
// longitude lines.
//
// According to Peter Dana (Geographer's Craft web pages), even when
// the major grid boundaries have been shifted, the meridian used
// for drawing the subgrids is still based on six-degree boundaries
// (as if the major grid hadn't been shifted at all).  That means we
// are drawing the subgrids correctly as it stands now for the
// irregular grids (31V/32V/31X/33X/35X/37X).  The irregular zones
// have sizes of 3/9/12 degrees (width) instead of 6 degrees.
//
// UTM NOTES:  84 degrees North to 80 degrees South. 60 zones, each
// covering six (6) degrees of longitude. Each zone extends three
// degrees eastward and three degrees westward from its central
// meridian.  Zones are numbered consecutively west to east from the
// 180 degree meridian. From 84 degrees North and 80 degrees South
// to the respective poles, the Universal Polar Stereographic (UPS)
// is used.
//
// For MGRS and UTM-Special grid only:
// UTM Zone 32 has been widened to 9° (at the expense of zone 31)
// between latitudes 56° and 64° (band V) to accommodate southwest
// Norway. Thus zone 32 extends westwards to 3°E in the North Sea.
// Similarly, between 72° and 84° (band X), zones 33 and 35 have
// been widened to 12° to accommodate Svalbard. To compensate for
// these 12° wide zones, zones 31 and 37 are widened to 9° and zones
// 32, 34, and 36 are eliminated. Thus the W and E boundaries of
// zones are 31: 0 - 9 E, 33: 9 - 21 E, 35: 21 - 33 E and 37: 33 -
// 42 E.
//
// UTM is depending on the ellipsoid and the datum used.  For our
// purposes, we're always using WGS84 ellipsoid and datum, so it's a
// non-issue.
//
// Horizontal bands corresponding to the NATO UTM/UPS lettering:
// Zones go from A (south pole) to Z (north pole).  South of -80 are
// zones A/B, north of +84 are zones Y/Z.  "I" and "O" are not used.
// Zones from C to W are 8 degrees high.  Zone X is 12 degrees high.
//
// We need these NATO letters or a N/S designator in order to
// specify which hemisphere the UTM coordinates are in.  Often, the
// same coordinates can appear in either hemisphere.  Some computer
// software uses +/- to designate northings instead of N/S or the
// NATO lettered bands.
//
// UPS system is used at the poles instead of UTM.  UPS uses a false
// northing and easting of 2,000,000 meters.
//
// An arbitrary false northing of 10,000,000 at the equator is used
// for southern latitudes only.  Northern latitudes assume the
// equator northing is at zero.  An arbitrary false easting of
// 500,000 is along the meridian of each zone (3 degrees from each
// side).  The lettered grid lines are necessary due to some
// coordinates being valid in both the northern and the southern
// hemisphere.
//
// Y/Z  84N to 90N (UPS System) false N/E = 2,000,000
// X    72N to 84N (12 degrees latitude, equator=0)
// W    64N to 72N ( 8 degrees latitude, equator=0)
// V    56N to 64N ( 8 degrees latitude, equator=0)
// U    48N to 56N ( 8 degrees latitude, equator=0)
// T    40N to 48N ( 8 degrees latitude, equator=0)
// S    32N to 40N ( 8 degrees latitude, equator=0)
// R    24N to 32N ( 8 degrees latitude, equator=0)
// Q    16N to 24N ( 8 degrees latitude, equator=0)
// P     8N to 16N ( 8 degrees latitude, equator=0)
// N     0N to  8N ( 8 degrees latitude, equator=0)
// M     0S to  8S ( 8 degrees latitude, equator=10,000,000)
// L     8S to 16S ( 8 degrees latitude, equator=10,000,000)
// K    16S to 24S ( 8 degrees latitude, equator=10,000,000)
// J    24S to 32S ( 8 degrees latitude, equator=10,000,000)
// H    32S to 40S ( 8 degrees latitude, equator=10,000,000)
// G    40S to 48S ( 8 degrees latitude, equator=10,000,000)
// F    48S to 56S ( 8 degrees latitude, equator=10,000,000)
// E    56S to 64S ( 8 degrees latitude, equator=10,000,000)
// D    64S to 72S ( 8 degrees latitude, equator=10,000,000)
// C    72S to 80S ( 8 degrees latitude, equator=10,000,000)
// A/B  80S to 90S (UPS System) false N/E = 2,000,000
//
void draw_major_utm_mgrs_grid(Widget w)
{
  int ii;

// need to move metadata to its own function and put it up after grids have been drawn.
//*******
  // variables for metadata in grid border
  long xx2, yy2;  // coordinates of screen corners used for metadata
  int border_width;           // Width of the border to draw labels into.
  double easting, northing;   // Values used in border metadata.
  int x, y;                   // Screen coordinates for border labels.
  char zone_str[10];
  char zone_str2[10];
  char metadata_datum[6];
  char grid_label[25];        // String to draw labels on grid lines
  char grid_label1[25];       // String to draw latlong metadata
  char top_label[180];        // String to draw metadata on top border

  // variables to support components of MGRS strings in the metadata
  char mgrs_zone[4] = "   ";   // MGRS zone letter
  char mgrs_eastingL[3] = "  ";
  char mgrs_northingL[3] = "  ";
  unsigned int int_utmEasting;
  unsigned int int_utmNorthing;
  char mgrs_space_string[4] = "   ";
//  int mgrs_single_digraph = FALSE; // mgrs_ul_digraph and mgrs_ur_digraph are the same.
  char mgrs_ul_digraph[3] = "  ";  // MGRS digraph for upper left corner of screen
  char mgrs_lr_digraph[3] = "  ";  // MGRS digraph for lower right corner of screen


//fprintf(stderr,"draw_major_utm_mgrs_grid start\n");

  if (!long_lat_grid) // We don't wish to draw a map grid
  {
    return;
  }

  // Vertical lines:

  // Draw the vertical vectors (except for the irregular regions
  // and the prime meridian).  The polar areas only have two zones
  // each, so we don't want to draw through those areas.

  for (ii = -180; ii < 0; ii += 6)
  {
    draw_vector_ll(w, -80.0,  (float)ii, 84.0,  (float)ii, gc_tint, pixmap_final, 0);
  }
  for (ii = 42; ii <= 180; ii += 6)
  {
    draw_vector_ll(w, -80.0,  (float)ii, 84.0,  (float)ii, gc_tint, pixmap_final, 0);
  }

  // Draw the short vertical vectors in the polar regions
  draw_vector_ll(w, -90.0, -180.0, -80.0, -180.0, gc_tint, pixmap_final, 0);
  draw_vector_ll(w, -90.0,  180.0, -80.0,  180.0, gc_tint, pixmap_final, 0);
  draw_vector_ll(w,  84.0, -180.0,  90.0, -180.0, gc_tint, pixmap_final, 0);
  draw_vector_ll(w,  84.0,  180.0,  90.0,  180.0, gc_tint, pixmap_final, 0);

  if (coordinate_system == USE_UTM_SPECIAL
      || coordinate_system == USE_MGRS)
  {
    // For MGRS, we need to draw irregular zones in certain
    // areas.

    // Draw the partial vectors from 80S to the irregular region
    draw_vector_ll(w, -80.0,    6.0,  56.0,    6.0, gc_tint, pixmap_final, 0);
    draw_vector_ll(w, -80.0,   12.0,  72.0,   12.0, gc_tint, pixmap_final, 0);
    draw_vector_ll(w, -80.0,   18.0,  72.0,   18.0, gc_tint, pixmap_final, 0);
    draw_vector_ll(w, -80.0,   24.0,  72.0,   24.0, gc_tint, pixmap_final, 0);
    draw_vector_ll(w, -80.0,   30.0,  72.0,   30.0, gc_tint, pixmap_final, 0);
    draw_vector_ll(w, -80.0,   36.0,  72.0,   36.0, gc_tint, pixmap_final, 0);

    // Draw the short vertical vectors in the irregular region
    draw_vector_ll(w,  56.0,    3.0,  64.0,    3.0, gc_tint, pixmap_final, 0);
    draw_vector_ll(w,  64.0,    6.0,  72.0,    6.0, gc_tint, pixmap_final, 0);
    draw_vector_ll(w,  72.0,    9.0,  84.0,    9.0, gc_tint, pixmap_final, 0);
    draw_vector_ll(w,  72.0,   21.0,  84.0,   21.0, gc_tint, pixmap_final, 0);
    draw_vector_ll(w,  72.0,   33.0,  84.0,   33.0, gc_tint, pixmap_final, 0);

    // Draw the short vertical vectors above the irregular region
    draw_vector_ll(w,  84.0,    6.0,  84.0,    6.0, gc_tint, pixmap_final, 0);
    draw_vector_ll(w,  84.0,   12.0,  84.0,   12.0, gc_tint, pixmap_final, 0);
    draw_vector_ll(w,  84.0,   18.0,  84.0,   18.0, gc_tint, pixmap_final, 0);
    draw_vector_ll(w,  84.0,   24.0,  84.0,   24.0, gc_tint, pixmap_final, 0);
    draw_vector_ll(w,  84.0,   30.0,  84.0,   30.0, gc_tint, pixmap_final, 0);
    draw_vector_ll(w,  84.0,   36.0,  84.0,   36.0, gc_tint, pixmap_final, 0);
  }
  else
  {
    // Draw normal zone boundaries used for civilian UTM
    // grid.
    for (ii = 6; ii < 42; ii += 6)
    {
      draw_vector_ll(w, -80.0,  (float)ii, 84.0,  (float)ii, gc_tint, pixmap_final, 0);
    }
  }


  // Horizontal lines:

  // Draw the 8 degree spaced lines, except for the equator
  for (ii = -80; ii < 0; ii += 8)
  {
    draw_vector_ll(w, (float)ii, -180.0, (float)ii, 180.0, gc_tint, pixmap_final, 0);
  }
  // Draw the 8 degree spaced lines
  for (ii = 8; ii <= 72; ii += 8)
  {
    draw_vector_ll(w, (float)ii, -180.0, (float)ii, 180.0, gc_tint, pixmap_final, 0);
  }

  // Draw the one 12 degree spaced line
  draw_vector_ll(w, 84.0, -180.0, 84.0, 180.0, gc_tint, pixmap_final, 0);

  // Draw the pole lines
  draw_vector_ll(w, -90.0, -180.0, -90.0, 180.0, gc_tint, pixmap_final, 0);
  draw_vector_ll(w,  90.0, -180.0,  90.0, 180.0, gc_tint, pixmap_final, 0);

  // Set to solid line for the equator.  Make it extra wide as
  // well.
  (void)XSetLineAttributes (XtDisplay (w), gc_tint, 3, LineSolid, CapButt,JoinMiter);

  // Draw the equator as a solid line
  draw_vector_ll(w, 0.0, -180.0, 0.0, 180.0, gc_tint, pixmap_final, 0);

  (void)XSetLineAttributes (XtDisplay (w), gc_tint, 2, LineSolid, CapButt,JoinMiter);

  // Draw the prime meridian in the same manner
  draw_vector_ll(w, -80.0, 0.0, 84.0, 0.0, gc_tint, pixmap_final, 0);

  // add metadata and labels
  if (draw_labeled_grid_border==TRUE && scale_x > 3000)
  {
    // Determine the width of the border
    border_width = get_border_width(w);
    // Find out what the map datum is.
    get_horizontal_datum(metadata_datum, sizeof(metadata_datum));

    // Put metadata in top border.
    // find location of upper left corner of map, convert to UTM
    xx2 = NW_corner_longitude  + (border_width * scale_x);
    yy2 = NW_corner_latitude   + (border_width * scale_y);
    convert_xastir_to_UTM(&easting, &northing, zone_str, sizeof(zone_str),
                          xx2, yy2);
    if (coordinate_system == USE_MGRS)
    {
      convert_xastir_to_MGRS_str_components(mgrs_zone, strlen(mgrs_zone),
                                            mgrs_eastingL,   sizeof(mgrs_eastingL),
                                            mgrs_northingL,  sizeof(mgrs_northingL),
                                            &int_utmEasting, &int_utmNorthing,
                                            xx2, yy2,
                                            0, mgrs_space_string, strlen(mgrs_space_string));
      xastir_snprintf(mgrs_ul_digraph, sizeof(mgrs_ul_digraph),
                      "%c%c", mgrs_eastingL[0], mgrs_northingL[0]);
      xastir_snprintf(grid_label,
                      sizeof(grid_label),
                      "%s %s %05.0f %05.0f",
                      mgrs_zone,mgrs_ul_digraph,(float)int_utmEasting,(float)int_utmNorthing);
    }
    else
    {
      char easting_str[10];
      char northing_str[10];

      xastir_snprintf(easting_str, sizeof(easting_str), " %07.0f", easting);
      xastir_snprintf(northing_str, sizeof(northing_str), " %07.0f", northing);
      strcpy(grid_label, zone_str);
      grid_label[sizeof(grid_label)-1] = '\0';  // Terminate string
      strcat(grid_label, easting_str);
      grid_label[sizeof(grid_label)-1] = '\0';  // Terminate string
      strcat(grid_label, northing_str);
      grid_label[sizeof(grid_label)-1] = '\0';  // Terminate string
    }
    // find location of lower right corner of map, convert to UTM
    xx2 = NW_corner_longitude  + ((screen_width - border_width) * scale_x);
    yy2 = NW_corner_latitude   + ((screen_height - border_width) * scale_y);
    convert_xastir_to_UTM(&easting, &northing, zone_str, sizeof(zone_str),
                          xx2, yy2);
    if (coordinate_system == USE_MGRS)
    {
      convert_xastir_to_MGRS_str_components(mgrs_zone, strlen(mgrs_zone),
                                            mgrs_eastingL,   sizeof(mgrs_eastingL),
                                            mgrs_northingL,  sizeof(mgrs_northingL),
                                            &int_utmEasting, &int_utmNorthing,
                                            xx2, yy2,
                                            0, mgrs_space_string, strlen(mgrs_space_string));
      xastir_snprintf(mgrs_lr_digraph, sizeof(mgrs_lr_digraph),
                      "%c%c", mgrs_eastingL[0], mgrs_northingL[0]);
      xastir_snprintf(grid_label1,
                      sizeof(grid_label1),
                      "%s %s %05.0f %05.0f",
                      mgrs_zone,mgrs_lr_digraph,(float)int_utmEasting,(float)int_utmNorthing);
      if (strcmp(mgrs_lr_digraph,mgrs_ul_digraph)==0)
      {
//             mgrs_single_digraph = TRUE; // mgrs_ul_digraph and mgrs_ur_digraph are the same.
      }
      else
      {
//             mgrs_single_digraph = FALSE; // mgrs_ul_digraph and mgrs_ur_digraph are the same.
      }
    }
    else
    {
      char easting_str[10];
      char northing_str[10];

      xastir_snprintf(easting_str, sizeof(easting_str), " %07.0f", easting);
      xastir_snprintf(northing_str, sizeof(northing_str), " %07.0f", northing);
      strcpy(grid_label1, zone_str);
      grid_label1[sizeof(grid_label1)-1] = '\0';  // Terminate string
      strcat(grid_label1, easting_str);
      grid_label1[sizeof(grid_label1)-1] = '\0';  // Terminate string
      strcat(grid_label1, northing_str);
      grid_label1[sizeof(grid_label1)-1] = '\0';  // Terminate string
    }
    // Write metadata on upper border of map.
    //"XASTIR Map of %s (upper left) to %s (lower right).  UTM zones, %s datum. ",
    xastir_snprintf(top_label,
                    sizeof(top_label),
                    langcode("MDATA003"),
                    grid_label,grid_label1,metadata_datum);
    draw_rotated_label_text_to_target (w, 270,
                                       border_width+2,
                                       border_width-1,
                                       sizeof(top_label),colors[0x10],top_label,FONT_BORDER,
                                       pixmap_final,
                                       outline_border_labels, colors[outline_border_labels_color]);
    // Crudely identify zone boundaries by
    // iterating across bottom border.
    xastir_snprintf(zone_str2,
                    sizeof(zone_str2),
                    "%s"," ");
    for (x=1; x<(screen_width - border_width); x++)
    {
      xx2 = NW_corner_longitude  + (x * scale_x);
      yy2 = NW_corner_latitude   + ((screen_height - border_width) * scale_y);
      convert_xastir_to_UTM(&easting, &northing, zone_str, sizeof(zone_str),
                            xx2, yy2);
      zone_str[strlen(zone_str)-1] = '\0';
      if (strcmp(zone_str,zone_str2) !=0)
      {
        draw_rotated_label_text_to_target (w, 270,
                                           x + 1,
                                           screen_height,
                                           sizeof(zone_str),colors[0x10],zone_str,FONT_BORDER,
                                           pixmap_final,
                                           outline_border_labels, colors[outline_border_labels_color]);
      }
      xastir_snprintf(zone_str2,
                      sizeof(zone_str2),
                      "%s",zone_str);
    }
    // Crudely identify zone letters by iterating down left border
    for (y=(border_width*2); y<(screen_height - border_width); y++)
    {
      xx2 = NW_corner_longitude   + (border_width * scale_x);
      yy2 = NW_corner_latitude   + (y * scale_y);
      convert_xastir_to_UTM(&easting, &northing, zone_str, sizeof(zone_str),
                            xx2, yy2);
      zone_str[0] = zone_str[strlen(zone_str)-1];
      zone_str[1] = '\0';
      if (strcmp(zone_str,zone_str2) !=0)
      {
        draw_rotated_label_text_to_target (w, 270,
                                           1,
                                           y,
                                           sizeof(zone_str),colors[0x10],zone_str,FONT_BORDER,
                                           pixmap_final,
                                           outline_border_labels, colors[outline_border_labels_color]);
      }
      xastir_snprintf(zone_str2,
                      sizeof(zone_str2),
                      "%s",zone_str);
    }
  } // end if draw labeled border

  // Set the line width and style in the GC to 1 pixel wide for
  // drawing the smaller grid
  (void)XSetLineAttributes (XtDisplay (w), gc_tint, 1, LineOnOffDash, CapButt,JoinMiter);

//fprintf(stderr,"draw_major_utm_mgrs_grid end\n");

} // End of draw_major_utm_mgrs_grid()





// This is the function which actually draws a minor UTM grid.
// Called by draw_minor_utm_mgrs_grid() function below.
// draw_minor_utm_mgrs_grid() is the function which calculates the
// grid points.
//
void actually_draw_utm_minor_grid(Widget w)
{


  int border_width;           // Width of the border to draw labels into.
  int numberofzones = 0;      // number of elements in utm_grid.zone[] that are used
  int Zone;
  int ii;
  int easting_color;          // Colors for the grid labels
  int northing_color;
  int zone_color;             // zone label color
  int label_on_left;          // if true, draw northing labels on left
  long xx, yy, xx2, yy2;
  char zone_str[10];
  char zone_str2[10];
  double easting, northing;
  int short_width_pixels = 0; // Width of an unrotated string of five_zeroes in the border font in pixels.
  int string_width_pixels = 0;// Width of an unrotated string of seven_zeroes in the border font in pixels.
  char metadata_datum[6];
  char grid_label[25];        // String to draw labels on grid lines
  char grid_label1[25];       // String to draw latlong metadata
  char top_label[180];        // String to draw metadata on top border
  int grid_spacing_pixels;    // Spacing of fine grid lines in pixels.
  int bottom_point;           // utm_grid.zone[].col[].npoints can extend past the
  // bottom of the screen, this is the lowest point in the
  // points array that is on the screen.
  int  skip_alternate_label;  // Skip alternate easting and northing labels
  // if they would overlap on the
  // display.
  int last_line_labeled;      // Marks lines that were labeled
  // when alternate lines are not
  // being labeled.

  // variables to support components of MGRS strings
  char mgrs_zone[4] = "   ";   // MGRS zone letter
  char mgrs_eastingL[3] = "  ";
  char mgrs_northingL[3] = "  ";
  unsigned int int_utmEasting;
  unsigned int int_utmNorthing;
  char mgrs_space_string[4] = "   ";
  int mgrs_single_digraph = FALSE; // mgrs_ul_digraph and mgrs_ur_digraph are the same.
  char mgrs_ul_digraph[3] = "  ";  // MGRS digraph for upper left corner of screen
  char mgrs_lr_digraph[3] = "  ";  // MGRS digraph for lower right corner of screen

  if (!long_lat_grid) // We don't wish to draw a map grid
  {
    return;
  }

  // OLD: Draw grid in dashed white lines.
  // NEW: Tint the lines as they go along, making them appear
  // no matter what color is underneath.
  (void)XSetForeground(XtDisplay(w), gc_tint, colors[0x27]);

  // Note:  npoints can be negative here!  Make sure our code
  // checks for that.  Initially npoints was an unsigned int.
  // Changed it to an int so that we can get and check for
  // negative values, bypassing segfaults.
  //
  numberofzones = 0;

  // Determine the width of the border
  border_width = get_border_width(w);
  // Determine some parameters used in drawing the border.
  string_width_pixels = get_standard_border_string_width_pixels(w, 7);
  short_width_pixels = get_standard_border_string_width_pixels(w,5);

  for (Zone=0; Zone < UTM_GRID_MAX_ZONES; Zone++)
  {

    if (utm_grid.zone[Zone].ncols > 0)
    {
      // find out how many zones are actually drawn on the map
      numberofzones++;
    }
  }

  for (Zone=0; Zone < UTM_GRID_MAX_ZONES; Zone++)
  {

    for (ii=0; ii < (int)utm_grid.zone[Zone].ncols; ii++)
    {
      if (utm_grid.zone[Zone].col[ii].npoints > 1)
      {

        // We need to check for points that are more
        // than +/- 16383.  If we have any, it can cause
        // X11 to lock up for a while drawing lots of
        // extra lines, due to bugs in X11.  We do that
        // checking above with xx and yy.
        //
        (void)XDrawLines(XtDisplay(w),
                         pixmap_final,
                         gc_tint,
                         utm_grid.zone[Zone].col[ii].points,
                         l16(utm_grid.zone[Zone].col[ii].npoints),
                         CoordModeOrigin);
      }
    }

    for (ii=0; ii < (int)utm_grid.zone[Zone].nrows; ii++)
    {
      if (utm_grid.zone[Zone].row[ii].npoints > 1)
      {

        // We need to check for points that are more
        // than +/- 16383.  If we have any, it can cause
        // X11 to lock up for a while drawing lots of
        // extra lines, due to bugs in X11.  We do that
        // checking above with xx and yy.
        //
        (void)XDrawLines(XtDisplay(w),
                         pixmap_final,
                         gc_tint,
                         utm_grid.zone[Zone].row[ii].points,
                         l16(utm_grid.zone[Zone].row[ii].npoints),
                         CoordModeOrigin);
      }
    }

    // Check each of the 4 possible utm_grid.zone array elements
    // that might contain a grid, and label the grid if it exists.
    if (utm_grid.zone[Zone].nrows>0 && utm_grid.zone[Zone].ncols>0)
    {
      if (draw_labeled_grid_border==TRUE)
      {
        // Label the UTM grid on the border.
        // Since the coordinate of the current mouse pointer position is
        // continually updated, labeling the grid is primarily for the
        // purpose of printing maps and saving screenshots.
        //
        // ******* Doesn't work properly near poles when 3 zones are on screen
        // ******* (e.g. 13,14,15) - overlaps northings for 14 and 15.
        // ******* Doesn't clearly distinguish one zone with 2 lettered rows
        // ******* (e.g. 18T,18U) needs color distinction between northings
        // ******* to indicate which northings are in which lettered row.
        //

        // Default labels for just one zone on screen are black text for
        // zone at lower left corner, eastings on bottom, and northings
        // at right.
        // Idea is to normally start at the lower left corner
        // users can then easily follow left to right to get easting,
        // and bottom to top to get northing.
        // For two zones, second zone uses blue text for eastings and northings.
        easting_color = 0x08;  // black text
        northing_color = 0x08; // black text
        zone_color = 0x08;     // black text
        // 0x09=blue (0x0e=yellow works well with outline, but not without).
        label_on_left = FALSE;

        // Find out what the map datum is.
        get_horizontal_datum(metadata_datum, sizeof(metadata_datum));

        if (numberofzones>1)
        {
          // check to see if the upper left and lower left corners are in the same zone
          // if not, label the upper left corner
          xx = (border_width * scale_x) + NW_corner_longitude;
          yy = ((screen_height - border_width) * scale_y) + NW_corner_latitude;
          convert_xastir_to_UTM(&easting, &northing, zone_str, sizeof(zone_str), xx, yy);
          yy = (border_width * scale_y) + NW_corner_latitude;
          convert_xastir_to_UTM(&easting, &northing, zone_str2, sizeof(zone_str2), xx, yy);
          if (strcmp(zone_str,zone_str2)!=0)
          {
            xastir_snprintf(grid_label,
                            sizeof(grid_label),
                            "%s",
                            zone_str2);
            //draw_nice_string(w,pixmap_final,0,
            //    border_width+2,
            //    (2*border_width)+2,
            //    grid_label,
            //    0x10,zone_color,(int)strlen(grid_label));
            draw_rotated_label_text_to_target (w, 270,
                                               border_width+2,
                                               (2*border_width)+2,
                                               sizeof(grid_label),colors[zone_color],grid_label,FONT_BORDER,
                                               pixmap_final,
                                               1, colors[0x0f]);
          }
          if (strcmp(zone_str,zone_str2)!=0)
          {
            xastir_snprintf(grid_label,
                            sizeof(grid_label),
                            "%s",
                            zone_str);
            draw_rotated_label_text_to_target (w, 270,
                                               border_width+2,
                                               screen_height - (2*border_width) - 2,
                                               sizeof(grid_label),colors[zone_color],grid_label,FONT_BORDER,
                                               pixmap_final,
                                               1, colors[0x0f]);
          }
          zone_color = 0x09;
          // likewise for upper and lower right corners
          xx = ((screen_width - border_width) * scale_x) + NW_corner_longitude;
          yy = ((screen_height - border_width) * scale_y) + NW_corner_latitude;
          convert_xastir_to_UTM(&easting, &northing, zone_str, sizeof(zone_str), xx, yy);
          yy = (border_width * scale_y) + NW_corner_latitude;
          convert_xastir_to_UTM(&easting, &northing, zone_str2, sizeof(zone_str2), xx, yy);
          if (strcmp(zone_str,zone_str2)!=0)
          {
            xastir_snprintf(grid_label,
                            sizeof(grid_label),
                            "%s",
                            zone_str2);
            //draw_nice_string(w,pixmap_final,0,
            //    screen_width - (border_width * 3) ,
            //    (2*border_width)+2,
            //    grid_label,
            //    0x10,zone_color,(int)strlen(grid_label));
            draw_rotated_label_text_to_target (w, 270,
                                               screen_width - (border_width * 3),
                                               (2*border_width)+2,
                                               sizeof(grid_label),colors[zone_color],grid_label,FONT_BORDER,
                                               pixmap_final,
                                               1, colors[0x0f]);
          }
          if (strcmp(zone_str,zone_str2)!=0)
          {
            xastir_snprintf(grid_label,
                            sizeof(grid_label),
                            "%s",
                            zone_str);
            draw_rotated_label_text_to_target (w, 270,
                                               screen_width - (border_width * 3),
                                               screen_height - (2*border_width) - 2,
                                               sizeof(grid_label),colors[zone_color],grid_label,FONT_BORDER,
                                               pixmap_final,
                                               1, colors[0x0f]);
          }

          // are we currently the same zone as the upper left corner
          // if so, we need to place the northing labels on the left side
          xx = (utm_grid.zone[Zone].col[0].points[0].x * scale_x) + NW_corner_longitude;
          yy = (utm_grid.zone[Zone].col[0].points[0].y * scale_y) + NW_corner_latitude;
          convert_xastir_to_UTM(&easting, &northing, zone_str, sizeof(zone_str), xx, yy);
          convert_xastir_to_UTM(&easting, &northing, zone_str2, sizeof(zone_str2), NW_corner_longitude, NW_corner_latitude);
          if (strcmp(zone_str,zone_str2)==0)
          {
            northing_color = 0x08;  // 0x08 = black, same as lower left easting
            label_on_left = TRUE;
          }

        }
        // check to see if there is a horizontal boundary
        // compare xone of upper left and lower left corners
        convert_xastir_to_UTM(&easting, &northing, zone_str, sizeof(zone_str), NW_corner_longitude, NW_corner_latitude);

        // Overwrite defaults as appropriate and
        // label zones differently if more than one appears on the screen.

        if (Zone > 0)
        {
          // write the zone label on the bottom border
          zone_color = 0x09;     // blue
          easting_color = 0x09;  // blue
          northing_color = 0x09; // blue
          xx2 = utm_grid.zone[Zone].col[0].points[0].x;
          xx = (xx2 * scale_x) + NW_corner_longitude;
          yy2 = utm_grid.zone[Zone].col[0].points[utm_grid.zone[Zone].col[0].npoints-1].y;
          yy = (yy2 * scale_y) +  NW_corner_latitude;
          convert_xastir_to_UTM(&easting, &northing, zone_str, sizeof(zone_str), xx,yy);
          xastir_snprintf(grid_label,
                          sizeof(grid_label),
                          "%s",
                          zone_str);
          draw_rotated_label_text_to_target (w, 270,
                                             xx2,
                                             screen_height,
                                             sizeof(grid_label),colors[easting_color],grid_label,FONT_BORDER,
                                             pixmap_final,
                                             outline_border_labels, colors[outline_border_labels_color]);
          //draw_nice_string(w,pixmap_final,0,
          //    xx2,
          //    screen_height - 2,
          //    grid_label,
          //    0x10,zone_color,(int)strlen(grid_label));
        }

        if (Zone==0)
        {
          // write the zone of the lower left corner of the map
          xx = (border_width * scale_x) + NW_corner_longitude;
          yy = ((screen_height - border_width) * scale_y) +  NW_corner_latitude;
          convert_xastir_to_UTM(&easting, &northing, zone_str, sizeof(zone_str), xx, yy);
          xastir_snprintf(grid_label,
                          sizeof(grid_label),
                          "%s",
                          zone_str);
          draw_rotated_label_text_to_target (w, 270,
                                             1,
                                             screen_height,
                                             sizeof(grid_label),colors[easting_color],grid_label,FONT_BORDER,
                                             pixmap_final,
                                             outline_border_labels, colors[outline_border_labels_color]);
          //draw_nice_string(w,pixmap_final,0,
          //    1,
          //    screen_height - 2,
          //    grid_label,
          //    0x10,0x20,(int)strlen(grid_label));
        }
        // Put metadata in top border.
        // find location of upper left corner of map, convert to UTM
        xx2 = NW_corner_longitude  + (border_width * scale_x);
        yy2 = NW_corner_latitude   + (border_width * scale_y);
        convert_xastir_to_UTM(&easting, &northing, zone_str, sizeof(zone_str),
                              xx2, yy2);
        if (coordinate_system == USE_MGRS)
        {
          convert_xastir_to_MGRS_str_components(mgrs_zone, strlen(mgrs_zone),
                                                mgrs_eastingL,   sizeof(mgrs_eastingL),
                                                mgrs_northingL,  sizeof(mgrs_northingL),
                                                &int_utmEasting, &int_utmNorthing,
                                                xx2, yy2,
                                                0, mgrs_space_string, strlen(mgrs_space_string));
          xastir_snprintf(mgrs_ul_digraph, sizeof(mgrs_ul_digraph),
                          "%c%c", mgrs_eastingL[0], mgrs_northingL[0]);
          xastir_snprintf(grid_label,
                          sizeof(grid_label),
                          "%s %s %05.0f %05.0f",
                          mgrs_zone,mgrs_ul_digraph,(float)int_utmEasting,(float)int_utmNorthing);
        }
        else
        {
          char easting_str[10];
          char northing_str[10];

          xastir_snprintf(easting_str, sizeof(easting_str), " %07.0f", easting);
          xastir_snprintf(northing_str, sizeof(northing_str), " %07.0f", northing);
          strcpy(grid_label, zone_str);
          grid_label[sizeof(grid_label)-1] = '\0';  // Terminate string
          strcat(grid_label, easting_str);
          grid_label[sizeof(grid_label)-1] = '\0';  // Terminate string
          strcat(grid_label, northing_str);
          grid_label[sizeof(grid_label)-1] = '\0';  // Terminate string
        }
        // find location of lower right corner of map, convert to UTM
        xx2 = NW_corner_longitude  + ((screen_width - border_width) * scale_x);
        yy2 = NW_corner_latitude   + ((screen_height - border_width) * scale_y);
        convert_xastir_to_UTM(&easting, &northing, zone_str, sizeof(zone_str),
                              xx2, yy2);
        if (coordinate_system == USE_MGRS)
        {
          convert_xastir_to_MGRS_str_components(mgrs_zone, strlen(mgrs_zone),
                                                mgrs_eastingL,   sizeof(mgrs_eastingL),
                                                mgrs_northingL,  sizeof(mgrs_northingL),
                                                &int_utmEasting, &int_utmNorthing,
                                                xx2, yy2,
                                                0, mgrs_space_string, strlen(mgrs_space_string));
          xastir_snprintf(mgrs_lr_digraph, sizeof(mgrs_lr_digraph),
                          "%c%c", mgrs_eastingL[0], mgrs_northingL[0]);
          xastir_snprintf(grid_label1,
                          sizeof(grid_label1),
                          "%s %s %05.0f %05.0f",
                          mgrs_zone,mgrs_lr_digraph,(float)int_utmEasting,(float)int_utmNorthing);
          if (strcmp(mgrs_lr_digraph,mgrs_ul_digraph)==0)
          {
            mgrs_single_digraph = TRUE; // mgrs_ul_digraph and mgrs_ur_digraph are the same.
          }
          else
          {
            mgrs_single_digraph = FALSE; // mgrs_ul_digraph and mgrs_ur_digraph are the same.
          }
        }
        else
        {
          char easting_str[10];
          char northing_str[10];

          xastir_snprintf(easting_str, sizeof(easting_str), " %07.0f", easting);
          xastir_snprintf(northing_str, sizeof(northing_str), " %07.0f", northing);
          strcpy(grid_label1, zone_str);
          grid_label1[sizeof(grid_label1)-1] = '\0';  // Terminate string
          strcat(grid_label1, easting_str);
          grid_label1[sizeof(grid_label1)-1] = '\0';  // Terminate string
          strcat(grid_label1, northing_str);
          grid_label1[sizeof(grid_label1)-1] = '\0';  // Terminate string
        }
        //"XASTIR Map of %s (upper left) to %s (lower right).  UTM %d m grid, %s datum. ",
        xastir_snprintf(top_label,
                        sizeof(top_label),
                        langcode("MDATA001"),
                        grid_label,grid_label1,utm_grid_spacing_m,metadata_datum);
        //draw_nice_string(w,pixmap_final,0,
        //    border_width+2,
        //    border_width-2,
        //    top_label,
        //    0x10,0x20,(int)strlen(top_label));
        draw_rotated_label_text_to_target (w, 270,
                                           border_width+2,
                                           border_width-1,
                                           sizeof(top_label),colors[0x10],top_label,FONT_BORDER,
                                           pixmap_final,
                                           outline_border_labels, colors[outline_border_labels_color]);

        // deterimne whether the easting and northing strings will fit
        // in a grid box, or whether easting strings in adjacent boxes
        // will overlap (so that alternate strings can be skipped).
        if (utm_grid.zone[Zone].ncols > 1)
        {
          // find out the number of pixels beteen two grid lines
          grid_spacing_pixels =
            utm_grid.zone[Zone].col[1].points[0].x  -
            utm_grid.zone[Zone].col[0].points[0].x;

          if (grid_spacing_pixels == 0)
          {
            grid_spacing_pixels = -1;  // Skip
          }

        }
        else
        {
          // only one column in this zone, skip alternate doesn't matter
          grid_spacing_pixels = -1;
        }

        // Is truncated easting or northing larger than grid spacing?
        // If so, skip alternate labels
        // short_width_pixels+2 seems to work well.
        if (short_width_pixels+2>grid_spacing_pixels)
        {
          skip_alternate_label = TRUE;
        }
        else
        {
          skip_alternate_label = FALSE;
        }

        // Label the grid lines on the border.
        // Put easting along the bottom for easier correct ordering of easting and northing
        // by people who are reading the map.
        last_line_labeled = FALSE;
        for (ii=1; ii < (int)utm_grid.zone[Zone].ncols; ii++)
        {
          // label meridianal grid lines with easting

          if (utm_grid.zone[Zone].col[ii].npoints > 1)
          {

            // adjust up in case npoints goes far below the screen
            if (grid_spacing_pixels == 0)
            {
              continue;  // Go to next iteration of for loop
            }

            bottom_point = (int)(screen_height/grid_spacing_pixels);

            if (bottom_point >= utm_grid.zone[Zone].col[ii].npoints)
            {
              bottom_point = utm_grid.zone[Zone].col[ii].npoints - 1;
            }
            if (skip_alternate_label==TRUE && last_line_labeled==TRUE)
            {
              last_line_labeled = FALSE;
            }
            else
            {
              xx = (utm_grid.zone[Zone].col[ii].points[bottom_point].x * scale_x) + NW_corner_longitude;
              yy = (utm_grid.zone[Zone].col[ii].points[bottom_point].y * scale_y) + NW_corner_latitude;
              convert_xastir_to_UTM(&easting, &northing, zone_str, sizeof(zone_str), xx, yy);
              // To display full precision to one meter, use:
              //xastir_snprintf(grid_label,
              //    sizeof(grid_label),
              //    "%06.0f0",
              //    (float)((utm_grid_spacing_m/10) * roundf(easting/(utm_grid_spacing_m))));
              //
              // Divide easting by utm_grid_spacing to make sure the line is labeled
              // correctly, and not a few meters off, and truncate to at least 100 m.
              xastir_snprintf(grid_label,
                              sizeof(grid_label),
                              "%05.0f",
                              (float)((utm_grid_spacing_m/100) * roundf(easting/(utm_grid_spacing_m))));
              // truncate the label to an appropriate level of precision for the grid
              if (utm_grid_spacing_m ==1000)
              {
                grid_label[4] = ' ';
              }
              if (utm_grid_spacing_m ==10000)
              {
                grid_label[3] = ' ';
                grid_label[4] = ' ';
              }
              if (utm_grid_spacing_m ==100000)
              {
                grid_label[2] = ' ';
                grid_label[3] = ' ';
                grid_label[4] = ' ';
              }
              if (coordinate_system == USE_MGRS)
              {
                convert_xastir_to_MGRS_str_components(mgrs_zone, strlen(mgrs_zone),
                                                      mgrs_eastingL,   sizeof(mgrs_eastingL),
                                                      mgrs_northingL,  sizeof(mgrs_northingL),
                                                      &int_utmEasting, &int_utmNorthing,
                                                      xx, yy,
                                                      0, mgrs_space_string, strlen(mgrs_space_string));
                grid_label[0] = mgrs_eastingL[0];
                grid_label[1] = mgrs_northingL[0];
                if (mgrs_single_digraph==FALSE)
                {
                  grid_label[1] = '_';
                }
              }
              // draw each number at the bottom of the screen just to the right of the
              // relevant grid line at its location at the bottom of the screen
              //draw_nice_string(w,pixmap_final,0,
              //    utm_grid.zone[Zone].col[i].points[bottom_point].x+1,
              //    screen_height-2,
              //    grid_label,
              //    0x10,easting_color,(int)strlen(grid_label));

              // Don't overwrite the zone label, half the seven zeros string should give it room.
              // Don't draw the label if it will go off the left edge fo the screen.
              if ((utm_grid.zone[Zone].col[ii].points[bottom_point].x+1 > (string_width_pixels/2))
                  && (utm_grid.zone[Zone].col[ii].points[bottom_point].x+1 < (screen_width - string_width_pixels))
                 )
              {
                // ok to draw the label
                last_line_labeled = TRUE;
                draw_rotated_label_text_to_target (w, 270,
                                                   utm_grid.zone[Zone].col[ii].points[bottom_point].x+1,
                                                   screen_height,
                                                   sizeof(grid_label),colors[easting_color],grid_label,FONT_BORDER,
                                                   pixmap_final,
                                                   outline_border_labels, colors[outline_border_labels_color]);
              }
            }
          }
        }
        last_line_labeled = FALSE;
        // put northing along the right border, again for easier correct ordering of easting and northing.
        for (ii=0; ii < (int)utm_grid.zone[Zone].nrows; ii++)
        {
          // label latitudinal grid lines with northing
          if (utm_grid.zone[Zone].row[ii].npoints > 1)
          {
            if (skip_alternate_label==TRUE && last_line_labeled==TRUE)
            {
              last_line_labeled = FALSE;
            }
            else
            {
              if (label_on_left==TRUE)
              {
                xx = (utm_grid.zone[Zone].row[ii].points[0].x * scale_x) + NW_corner_longitude;
              }
              else
              {
                xx = (utm_grid.zone[Zone].row[ii].points[utm_grid.zone[Zone].row[ii].npoints-1].x * scale_x) + NW_corner_longitude;
              }
              yy = (utm_grid.zone[Zone].row[ii].points[utm_grid.zone[Zone].row[ii].npoints-1].y * scale_y) +  NW_corner_latitude;
              convert_xastir_to_UTM(&easting, &northing, zone_str, sizeof(zone_str), xx, yy);
              // To display to full 1 meter precision use:
              //xastir_snprintf(grid_label,
              //    sizeof(grid_label),
              //    "%06.0f0",
              //    (float)((utm_grid_spacing_m/10) * roundf(northing/(utm_grid_spacing_m))));
              //
              // Divide northing by utm grid spacing to make sure the line is labeled correctly
              // and displays zeroes in its least significant digits, and truncate to 100 m
              xastir_snprintf(grid_label,
                              sizeof(grid_label),
                              "%05.0f",
                              (float)((utm_grid_spacing_m/100) * roundf(northing/(utm_grid_spacing_m))));
              if (utm_grid_spacing_m ==1000)
              {
                grid_label[4] = ' ';
              }
              if (utm_grid_spacing_m ==10000)
              {
                grid_label[3] = ' ';
                grid_label[4] = ' ';
              }
              if (utm_grid_spacing_m ==100000)
              {
                grid_label[2] = ' ';
                grid_label[3] = ' ';
                grid_label[4] = ' ';
              }
              if (coordinate_system == USE_MGRS)
              {
                convert_xastir_to_MGRS_str_components(mgrs_zone, strlen(mgrs_zone),
                                                      mgrs_eastingL,   3,
                                                      mgrs_northingL,  3,
                                                      &int_utmEasting, &int_utmNorthing,
                                                      xx, yy,
                                                      0, mgrs_space_string, strlen(mgrs_space_string));
                grid_label[0] = mgrs_eastingL[0];
                if (mgrs_single_digraph==FALSE)
                {
                  grid_label[0] = '_';
                }
                grid_label[1] = mgrs_northingL[0];
              }
              // Draw northing labels.
              // Draw each number just above the relevant grid line along the right side
              // of the screen.  Don't write in the bottom border or off the top of the screen.
              if (label_on_left==TRUE)
              {
                // label northings on left border
                // don't overwrite the zone designator in  the lower left border
                if ((utm_grid.zone[Zone].row[ii].points[0].y < (screen_height - border_width))
                    &&
                    (utm_grid.zone[Zone].row[ii].points[0].y > (string_width_pixels))
                   )
                {
                  last_line_labeled = TRUE;
                  draw_rotated_label_text_to_target (w, 180,
                                                     border_width,
                                                     utm_grid.zone[Zone].row[ii].points[0].y,
                                                     sizeof(grid_label),colors[northing_color],grid_label,FONT_BORDER,
                                                     pixmap_final,
                                                     outline_border_labels, colors[outline_border_labels_color]);
                }
              }
              else
              {
                if (((utm_grid.zone[Zone].row[ii].points[utm_grid.zone[Zone].row[ii].npoints-1].y-1)
                     < (screen_height - border_width))
                    &&
                    ((utm_grid.zone[Zone].row[ii].points[utm_grid.zone[Zone].row[ii].npoints-1].y-1)
                     > (string_width_pixels))
                   )
                {
                  // label northings on right border
                  last_line_labeled = TRUE;
                  draw_rotated_label_text_to_target (w, 180,
                                                     screen_width,
                                                     utm_grid.zone[Zone].row[ii].points[utm_grid.zone[Zone].row[ii].npoints-1].y-1,
                                                     sizeof(grid_label),colors[northing_color],grid_label,FONT_BORDER,
                                                     pixmap_final,
                                                     outline_border_labels, colors[outline_border_labels_color]);
                }
              }
            }
          }
        } // for i=0 to nrows
      } // if draw labeled grid border
    } // if utm_grid.zone[Zone] is non-empty
  } // for each zone in utm_grid.zone
} // End of actually_draw_utm_minor_grid() function





// Calculate the minor UTM grids.  Called by draw_grid() below.
// This function calculates and caches a within-zone UTM grid
// for the current map view if one does not allready exist, it
// then calls actually_draw_utm_minor_grid() function above to do
// the drawing once the grid has been calculated.  Zone boundaries
// are drawn separately by draw_major_utm_mgrs_grid().
//
// This routine appears to draw most of the UTM/UPS grid ok, with
// the exceptions of:
//
// 1) Sometimes fails to draw vertical lines nearest zone
//    boundaries.
// 2) Lines connect across zone boundaries in an incorrect manner,
//    jumping up one grid interval across the boundary.
// 3) Segfaults near the special zone intersections as you zoom in.
//
// The code currently creates a col and row array per zone visible,
// with XPoints malloced that contain the grid intersections in
// screen coordinates.  If the screen is zoomed or panned they are
// recalculated.
//
// Perhaps we could do the same but with lat/long coordinates in the
// future so that we'd only have to recalculate when a new Zone came
// into view.  We'd use the lat/long vector drawing programs above
// then, with a possible slowdown due to more calculations if we're
// not moving around.
//
// Returns: 0 if successful or nothing to draw
//          1 if malloc error
//          2 if iterations error
//          3 if out of zones
//          4 if realloc failure
//
int draw_minor_utm_mgrs_grid(Widget w)
{

  long xx, yy, xx1, yy1;
  double e[4], n[4];
  char place_str[10], zone_str[10];
  int done = 0;
  int z1, z2, Zone, col, col_point, row, row_point, row_point_start;
  int iterations = 0;
  int finished_with_current_zone = 0;
  int ii, jj;
  float slope;
  int coordinate_system_backup = coordinate_system;


  col = 0;
  row = 0;
  col_point = 0;
  row_point = 0;
  row_point_start = 0;
  Zone = 0;

  // Set up for drawing zone grid(s)
  if (scale_x < 15)
  {
    utm_grid_spacing_m =    100;
  }
  else if (scale_x < 150)
  {
    utm_grid_spacing_m =   1000;
  }
  else if (scale_x < 1500)
  {
    utm_grid_spacing_m =  10000;
  }
  else if (scale_x < 3000)
  {
    utm_grid_spacing_m = 100000;
  }
  else
  {
    utm_grid_spacing_m = 0;
    // All done!  Don't draw the minor grids.  Major grids
    // have already been drawn by this point.
    return(0);
  }

  // Check hash to see if utm_grid is already set up
  if (utm_grid.hash.ul_x == NW_corner_longitude &&
      utm_grid.hash.ul_y == NW_corner_latitude &&
      utm_grid.hash.lr_x == SE_corner_longitude &&
      utm_grid.hash.lr_y == SE_corner_latitude)
  {

    // XPoint arrays are already set up.  Go draw the grid.
    actually_draw_utm_minor_grid(w);

    return(0);
  }


// If we get to this point, we need to re-create the minor UTM/MGRS
// grids as they haven't been set up yet or they don't match the
// current view.


  // Clear the minor UTM/MGRS grid arrays.  Alloc space for
  // the points in the grid structure.
  if (utm_grid_clear(1))
  {
    // If we got here, we had a problem with malloc's
    return(1);
  }

  // Find top left point of current view
  xx = NW_corner_longitude;
  yy = NW_corner_latitude;

  // Note that the minor grid depends on the STANDARD six degree
  // UTM zones, not the UTM-Special/MGRS zones.  Force our
  // calculations to use the standard zones.
  coordinate_system = USE_UTM;
  convert_xastir_to_UTM(&e[0], &n[0], place_str, sizeof(place_str), xx, yy);
  coordinate_system = coordinate_system_backup;

  n[0] += UTM_GRID_EQUATOR; // To work in southern hemisphere


// Select starting point, NW corner of NW zone

  // Move the coordinates to the nearest subgrid intersection,
  // based on our current grid spacing.  The grid intersection
  // we calculate here is northwest of our view's northwest
  // corner.
  e[0] /= utm_grid_spacing_m;
  e[0]  = (double)((int)e[0] * utm_grid_spacing_m);
  n[0] /= utm_grid_spacing_m;
  n[0]  = (double)((int)n[0] * utm_grid_spacing_m);
  n[0] += utm_grid_spacing_m;


//WE7U
// It appears that the horizontal grid lines get messed up in cases
// where the top horizontal line isn't in view on it's left end.
// That's a major clue!  Read the comment below (again with a "WE7U"
// tag).  The problem occurs at the point where we copy the last
// point from the previous grid over to the first point of a new
// grid.  That can cause us to be off by one, as for the grid on the
// left, the top horizontal line _is_ in view on the left.  We end
// up connecting the wrong horizontal lines together because of this
// mismatch, but again, only if the top horizontal line on the left
// grid is above the current view.
//
// It also appears that the vertical lines that are missing in some
// cases are on the right of the zone boundary.  This is probably
// because the top of that line doesn't go to the top of the view.
// On views where it does, the line is drawn.  I assume this is
// because we're drawing from NW corner to the right, and then down,
// which would cause that line to be skipped if it's not present on
// the first line?


  e[1] = e[0];
  n[1] = n[0];





/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
  // Start filling in the row/column arrays of grid intersections
  while (!done)
  {
    XPoint *temp_point;


    // Here's our escape in case we get stuck in this loop.
    // We can go through this loop multiple times for each
    // zone though, depending on our grid spacing.  64 rows * 64
    // colums * 8 points each = 32768, which gives us our upper
    // limit.
    if (iterations++ > 32768)
    {
      fprintf(stderr,
              "draw_minor_utm_mgrs_grid() looped too many times, escaping.\n");
      utm_grid_clear(1);
      return(2);
    }


    if (finished_with_current_zone)
    {
      // Set up to compute the next zone

      xx = NW_corner_longitude + ((utm_grid.zone[Zone].boundary_x + 1) * scale_x);

      yy = NW_corner_latitude;

      // Note that the minor grid depends on the STANDARD six
      // degree UTM zones, not the UTM-Special/MGRS zones.
      // Force our calculations to use the standard zones.
      coordinate_system = USE_UTM;
      convert_xastir_to_UTM(&e[0], &n[0], place_str, sizeof(place_str), xx, yy);
      coordinate_system = coordinate_system_backup;

      n[0] += UTM_GRID_EQUATOR; // To work in southern hemisphere

// Fix the coordinates to the nearest subgrid intersection based on
// our current grid spacing.  Bump both the easting and northing up
// by one subgrid.
      e[0] /= utm_grid_spacing_m;
      e[0]  = (double)((int)e[0] * utm_grid_spacing_m);
      e[0] += utm_grid_spacing_m;
      n[0] /= utm_grid_spacing_m;
      n[0]  = (double)((int)n[0] * utm_grid_spacing_m);
      n[0] += utm_grid_spacing_m;

      e[1] = e[0];
      n[1] = n[0];

#ifdef UTM_DEBUG
      fprintf(stderr,"\nFinished Zone=%d\n", Zone);
#endif

      // We're all done with the current zone.  Increment
      // to the next zone and set up to calculate its
      // points.
      Zone++;

#ifdef UTM_DEBUG
      fprintf(stderr,"\nstarting Zone=%d, row_point_start=1\n", Zone);
#endif

      row_point = row_point_start = 1;
      col = row = col_point = 0;
      finished_with_current_zone = 0;

      if (Zone >= UTM_GRID_MAX_ZONES)
      {
        fprintf(stderr,"Error: Zone=%d: out of zones!\n", Zone);
        Zone = 0;
        done = 1;
        utm_grid_clear(1);
        return(3);
      }
    }   // End of if(finished_with_current_zone)

    // Note that the minor grid depends on the STANDARD six
    // degree UTM zones, not the UTM-Special/MGRS zones.  Force
    // our calculations to use the standard zones.
    coordinate_system = USE_UTM;
    convert_UTM_to_xastir(e[1], n[1]-UTM_GRID_EQUATOR, place_str, &xx, &yy);
    coordinate_system = coordinate_system_backup;

    xx1 = xx; // Save
    yy1 = yy; // Save

    // Note that the minor grid depends on the STANDARD six
    // degree UTM zones, not the UTM-Special/MGRS zones.  Force
    // our calculations to use the standard zones.
    coordinate_system = USE_UTM;
    convert_xastir_to_UTM(&e[2], &n[2], zone_str, sizeof(zone_str), xx, yy);
    coordinate_system = coordinate_system_backup;

    n[2] += UTM_GRID_EQUATOR;
    xx = (xx - NW_corner_longitude) / scale_x;
    yy = (yy - NW_corner_latitude)  / scale_y;

    // Not all columns (and maybe rows) will start at point
    // 0
    if (utm_grid.zone[Zone].col[col].firstpoint == UTM_GRID_RC_EMPTY)
    {
      utm_grid.zone[Zone].col[col].firstpoint = l16(col_point);
#ifdef UTM_DEBUG
      fprintf(stderr,"col[%d] started at point %d\n", col, col_point);
#endif
    }
    if (utm_grid.zone[Zone].row[row].firstpoint == UTM_GRID_RC_EMPTY)
    {
      utm_grid.zone[Zone].row[row].firstpoint = l16(row_point);
#ifdef UTM_DEBUG
      fprintf(stderr,"row[%d] started at point %d\n", row, row_point);
#endif
    }

    // Check to see if we need to alloc more space for
    // column points
    ii = utm_grid.zone[Zone].col[col].npoints +
         utm_grid.zone[Zone].col[col].firstpoint + 1;
    if (ii > utm_grid.zone[Zone].col[col].nalloced)
    {
#ifdef UTM_DEBUG_ALLOC
      fprintf(stderr,"i=%d n=%d realloc(utm_grid.zone[%d].col[%d].points, ",
              ii, utm_grid.zone[Zone].col[col].nalloced, Zone, col);
#endif
      ii = ((ii / UTM_GRID_DEF_NALLOCED) + 1) * UTM_GRID_DEF_NALLOCED;
#ifdef UTM_DEBUG_ALLOC
      fprintf(stderr,"%d)\n", ii);
#endif

      temp_point = realloc(utm_grid.zone[Zone].col[col].points,
                           ii * sizeof(XPoint));

      if (temp_point)
      {
        utm_grid.zone[Zone].col[col].points = temp_point;
        utm_grid.zone[Zone].col[col].nalloced = ii;
      }
      else
      {
        puts("realloc FAILED!");
        (void)utm_grid_clear(1); // Clear arrays and allocate memory for points
        return(4);
      }
    }

    // Check to see if we need to alloc more space for row
    // points
    ii = utm_grid.zone[Zone].row[row].npoints +
         utm_grid.zone[Zone].row[row].firstpoint + 1;
    if (ii > utm_grid.zone[Zone].row[row].nalloced)
    {
#ifdef UTM_DEBUG_ALLOC
      fprintf(stderr,"i=%d n=%d realloc(utm_grid.zone[%d].row[%d].points, ",
              ii, utm_grid.zone[Zone].row[row].nalloced, Zone, row);
#endif
      ii = ((ii / UTM_GRID_DEF_NALLOCED) + 1) * UTM_GRID_DEF_NALLOCED;
#ifdef UTM_DEBUG_ALLOC
      fprintf(stderr,"%d)\n", ii);
#endif

      temp_point = realloc(utm_grid.zone[Zone].row[row].points,
                           ii * sizeof(XPoint));

      if (temp_point)
      {
        utm_grid.zone[Zone].row[row].points = temp_point;
        utm_grid.zone[Zone].row[row].nalloced = ii;
      }
      else
      {
        puts("realloc FAILED!");
        (void)utm_grid_clear(1); // Clear arrays and allocate memory for points
        return(4);
      }
    }

    // Here we check to see whether we are inserting points
    // that are greater than about +/- 32767.  If so,
    // truncate at that.  This prevents XDrawLines() from
    // going nuts and drawing hundreds of extra lines.
    //
    xx = l16(xx);
    yy = l16(yy);

    utm_grid.zone[Zone].col[col].points[col_point].x = l16(xx);
    utm_grid.zone[Zone].col[col].points[col_point].y = l16(yy);
    utm_grid.zone[Zone].col[col].npoints++;
    utm_grid.zone[Zone].row[row].points[row_point].x = l16(xx);
    utm_grid.zone[Zone].row[row].points[row_point].y = l16(yy);
    utm_grid.zone[Zone].row[row].npoints++;

#ifdef UTM_DEBUG
    fprintf(stderr,"utm_grid.zone[%d].col[%d].points[%d] = [ %ld,%ld ] npoints=%d\n",
            Zone, col, col_point, xx, yy, utm_grid.zone[Zone].col[col].npoints);
    fprintf(stderr,"utm_grid.zone[%d].row[%d].points[%d] = [ %ld,%ld ]\n",
            Zone, row, row_point, xx, yy);
#endif

    col++;
    row_point++;
    if (col >= UTM_GRID_MAX_COLS_ROWS)
    {
      finished_with_current_zone++;
    }

    z1 = atoi(place_str);
    z2 = atoi(zone_str);
    if (z1 != z2 || xx > screen_width)   // We hit a boundary
    {

#ifdef UTM_DEBUG_VERB
      if (z1 != z2)
      {
        fprintf(stderr,"Zone boundary! \"%s\" -> \"%s\"\n", place_str, zone_str);
      }
      else
      {
        puts("Screen boundary!");
      }
#endif

//#warning
//#warning I suspect that I should not use just col for the following.
//#warning
      if (col-2 >= 0)
        slope = (float)(yy - utm_grid.zone[Zone].col[col-2].points[col_point].y) /
                (float)(xx - utm_grid.zone[Zone].col[col-2].points[col_point].x + 0.001);
      else
      {
        slope = 0.0;
      }

      if (xx > screen_width)
      {
        xx1 = screen_width;
      }
      else
      {

        // 360,000 Xastir units equals one degree.  This
        // code appears to be adjusting xx1 to a major
        // zone edge.
        xx1 = (xx1 / (6 * 360000)) * 6 * 360000;
        xx1 = (xx1 - NW_corner_longitude) / scale_x;
      }

      utm_grid.zone[Zone].boundary_x = xx1;
      yy1 = yy - (xx - xx1) * slope;

#ifdef UTM_DEBUG
      fprintf(stderr,"_tm_grid.zone[%d].col[%d].points[%d] =  [ %ld,%ld ]\n",
              Zone, col-1, col_point, xx1, yy1);
      fprintf(stderr,"_tm_grid.zone[%d].row[%d].points[%d] =  [ %ld,%ld ]\n",
              Zone, row, row_point-1, xx1, yy1);
#endif

      if (col-1 >= 0 && row_point-1 >= 0)
      {
        utm_grid.zone[Zone].col[col-1].points[col_point].x = l16(xx1);
        utm_grid.zone[Zone].col[col-1].points[col_point].y = l16(yy1);
        utm_grid.zone[Zone].row[row].points[row_point-1].x = l16(xx1);
        utm_grid.zone[Zone].row[row].points[row_point-1].y = l16(yy1);
        if (z1 != z2 && Zone+1 < UTM_GRID_MAX_ZONES)
        {
          // copy over last points to start off new
          // zone
#ifdef UTM_DEBUG
          fprintf(stderr,"ztm_grid.zone[%d].row[%d].points[%d] =  [ %ld,%ld ]\n",
                  Zone+1, row, 0, xx1, yy1);
#endif

//WE7U
// This is where we can end up linking up/down one grid width
// between zones!!!  Without it though, we end up have a blank
// section to the right of the zone boundary.  Perhaps we could do
// this here, but when we get the next points calculated, we could
// check to see if we're off by about one grid width in the vertical
// direction.  If so, shift the initial point by that amount?
//
// Another possibility might be to draw bottom-to-top if in northern
// hemisphere, and top-to-bottom if in southern hemisphere.  That
// way we'd have the max amount of lines present when we start, and
// some might peter out as we draw along N/S.  Looking at the
// southern hemisphere right now though, that method doesn't appear
// to work.  We get the same problems there even though we're
// drawing top to bottom.
//
          utm_grid.zone[Zone+1].row[row].points[0].x = l16(xx1);
          utm_grid.zone[Zone+1].row[row].points[0].y = l16(yy1);
          utm_grid.zone[Zone+1].row[row].firstpoint =  0;
          utm_grid.zone[Zone+1].row[row].npoints    =  1;
        }
      }


      // Check last built row to see if it is all off
      // screen
      finished_with_current_zone++; // Assume we're done with this zone
      for (ii=0; ii < utm_grid.zone[Zone].row[row].npoints; ii++)
      {
        if (utm_grid.zone[Zone].row[row].points[ii].y <= screen_height)
        {
          finished_with_current_zone = 0;  // Some points were within the zone, keep computing
        }
      }


      e[1]  = e[0];               // carriage return
      n[1] -= utm_grid_spacing_m; // line feed
// Yea, your comments are real funny Olivier...  Gets the point
// across though!


      row++;
      if (row >= UTM_GRID_MAX_COLS_ROWS)
      {
        finished_with_current_zone++;
      }

      utm_grid.zone[Zone].ncols = max_i(col, utm_grid.zone[Zone].ncols);
      utm_grid.zone[Zone].nrows = max_i(row, utm_grid.zone[Zone].nrows);
      col = 0;
      row_point = row_point_start;
      col_point++;

      if (n[1] < 0)
      {
        fprintf(stderr,"n[1] < 0\n");
        finished_with_current_zone++;
      }

      if (finished_with_current_zone && xx > screen_width)
      {
        done = 1;
      }

      // Go to next iteration of while loop (skip next statement)
      continue;
    }

    e[1] += utm_grid_spacing_m;

  }   // End of while (done) loop

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////



//fprintf(stderr, "After while loop\n");

  // utm_grid.zone[] now contains an array of points marking fine grid
  // line intersections for parts of 1 to 4 zones that appear on
  // the screen.  Each utm_grid.zone[] is a vertical stripe, and may include
  // more than one zone letter, e.g. zone[0] might include 15U and 15T,
  // while zone[1] might include 16U and 16T.

//#define UTM_DEBUG_VERB

  for (Zone=0; Zone < UTM_GRID_MAX_ZONES; Zone++)
  {

#ifdef UTM_DEBUG_VERB
    fprintf(stderr,"\nutm_grid.zone[%d].ncols=%d\nutm_grid.zone[%d].nrows=%d\n",
            Zone, utm_grid.zone[Zone].ncols, Zone, utm_grid.zone[Zone].nrows);
#endif

    // Cleanup columns
    for (ii=0; ii < (int)utm_grid.zone[Zone].ncols; ii++)
    {
      int np = utm_grid.zone[Zone].col[ii].npoints;
      int fp = utm_grid.zone[Zone].col[ii].firstpoint;
      int nbp = 0;

#ifdef UTM_DEBUG_VERB
      fprintf(stderr,"utm_grid.zone[%d].col[%d].npoints=%d .firstpoint=%d\n",
              Zone, ii, np, fp);
      if (np < 2)
      {
        puts(" Not enough points!");
      }
      else
      {
        puts("");
      }

      for (jj=fp; jj < fp+np; jj++)
      {
        fprintf(stderr,"         col[%d].points[%d] = [ %d, %d ]", ii, jj,
                utm_grid.zone[Zone].col[ii].points[jj].x,
                utm_grid.zone[Zone].col[ii].points[jj].y);
        if (utm_grid.zone[Zone].col[ii].points[jj].x ==
            utm_grid.zone[Zone].boundary_x)
        {
          puts(" Boundary");
        }
        else
        {
          puts("");
        }
      }
#endif
      for (jj=fp; jj < fp+np; jj++)
      {
        if (utm_grid.zone[Zone].col[ii].points[jj].x ==
            utm_grid.zone[Zone].boundary_x)
        {
          nbp++;
        }
        else if (nbp > 0)   // We had a boundary point, but not anymore
        {
          fp = utm_grid.zone[Zone].col[ii].firstpoint = l16(jj - 1);
//fprintf(stderr,"np:%d, jj:%d\n",np,jj);
          // This can result in negative numbers!
          np = utm_grid.zone[Zone].col[ii].npoints = np - jj + 1;
//fprintf(stderr,"new np:%d\n",np);
          if (np < 0)
          {
            np = 0; // Prevents segfaults in
            // XDrawLines() and memmove()
            // below.
          }
          break;  // Exit from for loop
        }
        if (nbp == np)   // All points are boundary points
        {
          fp = utm_grid.zone[Zone].col[ii].firstpoint = 0;
          np = utm_grid.zone[Zone].col[ii].npoints    = 0;
        }
      }

// What's the below code doing?  Can get a segfault without this in
// the XDrawLines() functions below (fixed by making npoints an int
// instead of an unsigned int).  Sometimes we get a segfault right
// here due to the memmove() function.  In one such case, np was -2.
// Latest code keeps some lines from getting drawn, but at least we
// don't get a segfault.
//
      if (fp > 0)
      {
        if (np > 0)
        {
          memmove(&utm_grid.zone[Zone].col[ii].points[0],
                  &utm_grid.zone[Zone].col[ii].points[fp], np * sizeof(XPoint));
          fp = utm_grid.zone[Zone].col[ii].firstpoint = 0;
        }
        else
        {
//fprintf(stderr,"draw_minor_utm_mgrs_grid: ii:%d, np:%d, size:%d\n",ii,np,sizeof(XPoint));
//fprintf(stderr,"Problem1: in draw_minor_utm_mgrs_grid() memmove, np was %d.  Skipping memmove.\n",np);
        }
      }

#ifdef UTM_DEBUG_VERB
      fprintf(stderr,"_tm_grid.zone[%d].col[%d].npoints=%d.firstpoint=%d\n",
              Zone, ii, np, fp);
      for (jj=fp; jj < fp+np; jj++)
      {
        fprintf(stderr,"         col[%d].points[%d] = [ %d, %d ]", ii, jj,
                utm_grid.zone[Zone].col[ii].points[jj].x,
                utm_grid.zone[Zone].col[ii].points[jj].y);
        if (utm_grid.zone[Zone].col[ii].points[jj].x ==
            utm_grid.zone[Zone].boundary_x)
        {
          puts(" Boundary");
        }
        else
        {
          puts("");
        }
      }
      puts("");
#endif
    }

    // Cleanup rows
    for (ii=0; ii < (int)utm_grid.zone[Zone].nrows; ii++)
    {
      int np = utm_grid.zone[Zone].row[ii].npoints;
      int fp = utm_grid.zone[Zone].row[ii].firstpoint;
#ifdef UTM_DEBUG_VERB
      fprintf(stderr,"utm_grid.zone[%d].row[%d].npoints=%d.firstpoint=%d\n",
              Zone, ii, np, fp);
      if (np < 2)
      {
        puts(" Not enough points!");
      }
      else
      {
        puts("");
      }
#endif
// What's this doing?  This appears to be important, as things get
// really messed up if it's commented out.
      if (fp > 0)
      {
        if (np > 0)
        {
          memmove(&utm_grid.zone[Zone].row[ii].points[0],
                  &utm_grid.zone[Zone].row[ii].points[fp], np * sizeof(XPoint));
          fp = utm_grid.zone[Zone].row[ii].firstpoint = 0;
        }
        else
        {
//fprintf(stderr,"draw_minor_utm_mgrs_grid: ii:%d, np:%d, size:%d\n",ii,np,sizeof(XPoint));
//fprintf(stderr,"Problem2: in draw_minor_utm_mgrs_grid() memmove, np was %d.  Skipping memmove.\n",np);
        }

      }
#ifdef UTM_DEBUG_VERB
      for (jj=fp; jj < fp+np; jj++)
      {
        fprintf(stderr,"         row[%d].points[%d] = [ %d, %d ]\n", ii, jj,
                utm_grid.zone[Zone].row[ii].points[jj].x,
                utm_grid.zone[Zone].row[ii].points[jj].y);
      }
#endif
    }
  }

  // Rows and columns ready to go so setup hash
  utm_grid.hash.ul_x = NW_corner_longitude;
  utm_grid.hash.ul_y = NW_corner_latitude;
  utm_grid.hash.lr_x = SE_corner_longitude;
  utm_grid.hash.lr_y = SE_corner_latitude;

  // XPoint arrays are set up.  Go draw the grid.
  actually_draw_utm_minor_grid(w);

  return(0);

}   // End of draw_minor_utm_mgrs_grid() function





//*****************************************************************
// draw_grid()
//
// Draws a lat/lon or UTM/UPS grid on top of the view.
//
//*****************************************************************
void draw_grid(Widget w)
{
  int half;                   // Center of the white lines used to draw the borders
  int border_width = 14;      // The width of the border to draw around the
  // map to place labeled tick marks into
  // should be an even number.
  // The default here is overidden by the border fontsize.


  if (!long_lat_grid) // We don't wish to draw a map grid
  {
    return;
  }

  if (draw_labeled_grid_border==TRUE)
  {
    // Determine how wide the border should be.
    border_width = get_border_width(w);
    half = border_width/2;
    // draw a white border around the map.
    (void)XSetLineAttributes(XtDisplay(w),
                             gc,
                             border_width,
                             LineSolid,
                             CapRound,
                             JoinRound);
    (void)XSetForeground(XtDisplay(w),
                         gc,
                         colors[border_foreground_color]);         // white
    (void)XDrawLine(XtDisplay(w),
                    pixmap_final,
                    gc,
                    0,
                    l16(half),
                    l16(screen_width),
                    l16(half));
    (void)XDrawLine(XtDisplay(w),
                    pixmap_final,
                    gc,
                    l16(half),
                    0,
                    l16(half),
                    l16(screen_height));
    (void)XDrawLine(XtDisplay(w),
                    pixmap_final,
                    gc,
                    0,
                    l16(screen_height-half),
                    l16(screen_width),
                    l16(screen_height-half));
    (void)XDrawLine(XtDisplay(w),
                    pixmap_final,
                    gc,
                    l16(screen_width-half),
                    0,
                    l16(screen_width-half),
                    l16(screen_height));
  }

  // Set the line width in the GC to 2 pixels wide for the larger
  // UTM grid and the complete Lat/Long grid.
  (void)XSetLineAttributes (XtDisplay (w), gc_tint, 2, LineOnOffDash, CapButt,JoinMiter);
  (void)XSetForeground (XtDisplay (w), gc_tint, colors[0x27]);
  (void)XSetFunction (XtDisplay (da), gc_tint, GXxor);

  if (coordinate_system == USE_UTM
      || coordinate_system == USE_UTM_SPECIAL
      || coordinate_system == USE_MGRS)
  {

    int ret_code;

//draw_vector_ll(w, -5.0, -5.0,  5.0,  5.0, gc_tint, pixmap_final, 0);
//draw_vector_ll(w,  5.0,  5.0, -5.0, -5.0, gc_tint, pixmap_final, 0);

    // Draw major UTM/MGRS zones
    draw_major_utm_mgrs_grid(w);

    // Draw minor UTM/MGRS zones
    ret_code = draw_minor_utm_mgrs_grid(w);
    if (ret_code)
    {
      fprintf(stderr,
              "Encountered problem %d while calculating minor utm grid!\n",
              ret_code);
    }

  }   // End of UTM grid section
  else   // Lat/Long coordinate system, draw lat/long lines
  {
    draw_complete_lat_lon_grid(w);
  }   // End of Lat/Long section
}  // End of draw_grid()





/**********************************************************
 * get_map_ext()
 *
 * Returns the extension for the filename.  We use this to
 * determine which sort of map file it is.
 **********************************************************/
char *get_map_ext (char *filename)
{
  int len;
  int i;
  char *ext;

  ext = NULL;
  len = (int)strlen (filename);
  for (i = len; i >= 0; i--)
  {
    if (filename[i] == '.')
    {
      ext = filename + (i + 1);
      break;
    }
  }
  return (ext);
}





/**********************************************************
 * get_map_dir()
 *
 * Used to snag just the pathname from a complete filename.
 * Modifies input parameter "fullpath".
 **********************************************************/
char *get_map_dir (char *fullpath)
{
  int len;
  int i;

  len = (int)strlen (fullpath);
  for (i = len; i >= 0; i--)
  {
    if (fullpath[i] == '/')
    {
      fullpath[i + 1] = '\0';
      break;
    }
  }
  return (fullpath);
}





/***********************************************************
 * map_visible()
 *
 * Tests whether a particular path/filename is within our
 * current view.  We use this to decide whether to plot or
 * skip a particular image file (major speed-up!).
 * Input coordinates are in the Xastir coordinate system.
 *
 * Had to fix a bug here where the viewport glanced over the
 * edge of the earth, causing strange results like this.
 * Notice the View Edges Top value is out of range:
 *
 *
 *                Bottom         Top          Left       Right
 * View Edges:  31,017,956  4,290,923,492  35,971,339  90,104,075
 *  Map Edges:  12,818,482     12,655,818  64,079,859  64,357,110
 *
 * Left map boundary inside view
 * Right map boundary inside view
 * map_inside_view: 1  view_inside_map: 0  parallel_edges: 0
 * Map not within current view.
 * Skipping map: /usr/local/share/xastir/maps/tif/uk/425_0525_bng.tif
 *
 *
 * I had to check for out-of-bounds numbers for the viewport and
 * set them to min or max values so that this function always
 * works properly.  Here are the bounds of the earth (Xastir
 * Coordinate System):
 *
 *              0 (90 deg. or 90N)
 *
 * 0 (-180 deg. or 180W)      129,600,000 (180 deg. or 180E)
 *
 *          64,800,000 (-90 deg. or 90S)
 *
 ***********************************************************/
int map_visible (unsigned long map_max_y,   // bottom_map_boundary
                 unsigned long map_min_y,   // top_map_boundary
                 unsigned long map_min_x,   // left_map_boundary
                 unsigned long map_max_x)   // right_map_boundary) {
{

  //fprintf(stderr,"map_visible\n");

  // From computation geometry equations, intersection of two line
  // segments, they use the bounding box for two lines.  This is
  // the same as what we want to do:
  //
  // http://www.cs.kent.edu/~dragan/AdvAlg/CompGeom-2x1.pdfa
  // http://www.gamedev.net/reference/articles/article735.asp
  //
  // The quick rejection algorithm:
  //
  if (NW_corner_latitude > (long)map_max_y)
  {
    if (debug_level & 16)
    {
      fprintf(stderr,
              "map_visible, rejecting: NW_corner_latitude:%ld > map_max_y:%ld\n",
              NW_corner_latitude,
              map_max_y);
      fprintf(stderr,
              "\tmap or object is above viewport\n");
    }
    return(0);
  }

  if ((long)map_min_y > SE_corner_latitude)
  {
    if (debug_level & 16)
    {
      fprintf(stderr,
              "map_visible, rejecting: map_min_y:%ld > SE_corner_latitude:%ld\n",
              map_min_y,
              SE_corner_latitude);
      fprintf(stderr,
              "\tmap or object is below viewport\n");
    }
    return(0);
  }

  if (NW_corner_longitude > (long)map_max_x)
  {
    if (debug_level & 16)
    {
      fprintf(stderr,
              "map_visible, rejecting: NW_corner_longitude:%ld > map_max_x:%ld\n",
              NW_corner_longitude,
              map_max_x);
      fprintf(stderr,
              "\tmap or object is left of viewport\n");
    }
    return(0);
  }

  if ((long)map_min_x > SE_corner_longitude)
  {
    if (debug_level & 16)
    {
      fprintf(stderr,
              "map_visible, rejecting: map_min_x:%ld > SE_corner_longitude:%ld\n",
              map_min_x,
              SE_corner_longitude);
      fprintf(stderr,
              "\tmap or object is right of viewport\n");
    }
    return(0);
  }

  return (1); // At least part of the map is on-screen
}



/////////////////////////////////////////////////////////////////////
// get_viewport_lat_lon(double *xmin, double *ymin, double* xmax, double *ymax)
// Simply returns the floating point corners of the map display.
/////////////////////////////////////////////////////////////////////
void get_viewport_lat_lon(double *xmin,
                          double *ymin,
                          double* xmax,
                          double *ymax)
{

  *xmin=(double)f_NW_corner_longitude;
  *ymin=(double)f_SE_corner_latitude;
  *xmax=(double)f_SE_corner_longitude;
  *ymax=(double)f_NW_corner_latitude;
}

/////////////////////////////////////////////////////////////////////
// map_inside_viewport_lat_lon()
//  Returns 1 if the given set of xmin,xmax, ymin,ymax defines a
//  rectangle entirely contained in the current viewport (as opposed to
//  merely partially overlapping it.  Returns zero otherwise.
/////////////////////////////////////////////////////////////////////
int map_inside_viewport_lat_lon(double map_min_y,
                                double map_max_y,
                                double map_min_x,
                                double map_max_x)
{
  int retval=0;
  if (map_min_x >= f_NW_corner_longitude &&
      map_min_y >= f_SE_corner_latitude &&
      map_max_x <= f_SE_corner_longitude &&
      map_max_y <= f_NW_corner_latitude)
  {
    retval=1;
  }

  return (retval);
}


/////////////////////////////////////////////////////////////////////
// map_visible_lat_lon()
//
// We have the center of the view in floating point format:
//
//   float f_center_longitude; // Floating point map center longitude
//   float f_center_latitude;  // Floating point map center latitude
//
// So we just need to compute the top/bottom/left/right using those
// values and the scale_x/scale_y values before doing the compare.
//
// y scaling in 1/100 sec per pixel
// x scaling in 1/100 sec per pixel, calculated from scale_y
//
//
//              0 (90 deg. or 90N)
//
// 0 (-180 deg. or 180W)      129,600,000 (180 deg. or 180E)
//
//          64,800,000 (-90 deg. or 90S)
//
// *******************                  ******************* max_y
// *NW               * +                *                 * +
// *                 *                  *                 *
// *                 *                  *                 *
// *      View       * latitude (y)     *       Map       *
// *                 *                  *                 *
// *                 *                  *                 *
// *               SE* -                *                 * -
// *******************                  ******************* min_y
// -  longitude(x)   +                  - min_x     max_x +

/////////////////////////////////////////////////////////////////////
int map_visible_lat_lon (double map_min_y,    // f_bottom_map_boundary
                         double map_max_y,    // f_top_map_boundary
                         double map_min_x,    // f_left_map_boundary
                         double map_max_x)    // f_right_map_boundary
{

//fprintf(stderr,"map_visible_lat_lon\n");

  // From computation geometry equations, intersection of two line
  // segments, they use the bounding box for two lines.  This is
  // the same as what we want to do:
  //
  // http://www.cs.kent.edu/~dragan/AdvAlg/CompGeom-2x1.pdfa
  // http://www.gamedev.net/reference/articles/article735.asp
  //
  // The quick rejection algorithm:
  //
  if (map_max_y < f_SE_corner_latitude )
  {
    return(0);  // map below view
  }
  if (map_max_x < f_NW_corner_longitude)
  {
    return(0);  // map left of view
  }
  if (map_min_y > f_NW_corner_latitude )
  {
    return(0);  // view below map
  }
  if (map_min_x > f_SE_corner_longitude)
  {
    return(0);  // view left of  map
  }

  return (1); // Draw this map onto the screen
}





/**********************************************************
 * draw_label_text()
 *
 * Does what it says.  Used to draw strings onto the
 * display.
 **********************************************************/
void draw_label_text (Widget w, int x, int y, int label_length, int color, char *label_text)
{

  // This draws a gray background rectangle upon which we draw the text.
  // Probably not needed.  It ends up obscuring details underneath.
  //(void)XSetForeground (XtDisplay (w), gc, colors[0x0ff]);
  //(void)XFillRectangle (XtDisplay (w), pixmap, gc, x - 1, (y - 10),(label_length * 6) + 2, 11);

  (void)XSetForeground (XtDisplay (w), gc, color);
  (void)XDrawString (XtDisplay (w), pixmap, gc, x, y, label_text, label_length);
}





// Must make sure that fonts are not loaded again and again, as this
// takes a big chunk of memory each time.  Can you say "memory
// leak"?

XFontStruct *rotated_label_font[FONT_MAX]= {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
char rotated_label_fontname[FONT_MAX][MAX_LABEL_FONTNAME];
static char
current_rotated_label_fontname[FONT_MAX][sizeof(rotated_label_fontname)] = {"","","","","","","","",""};

/**********************************************************
 * draw_rotated_label_text_common()
 * call through wrappers:
 * draw_rotated_label_text_to_pixmap()
 * draw_rotated_label_text()
 * draw_centered_label_text()
 *
 * Does what it says.  Used to draw strings onto the
 * display.
 *
 * Use "xfontsel" or other tools to figure out what fonts
 * to use here.
 *
 * Paramenters:
 * target_pixmap specifies the pixmap the text is to be drawn to.
 * draw_outline specifies whether a 1 pixel outline around the
 *    text, TRUE to draw outline.
 * outline_bg_color is the color of the outline.
 * color is the color of the text inside the outline, or the
 *    color of the text itself if no outline is added.
 **********************************************************/
/* common code used by the two entries --- a result of retrofitting a new
   feature (centered) */
static void draw_rotated_label_text_common (Widget w, float my_rotation, int x, int y, int UNUSED(label_length), int color, char *label_text, int align, int fontsize, Pixmap target_pixmap, int draw_outline, int outline_bg_color)
{
//    XPoint *corner;
//    int i;
  int x_outline;
  int y_outline;


  // Do some sanity checking
  if (fontsize < 0 || fontsize >= FONT_MAX)
  {
    fprintf(stderr,"Font size is out of range: %d\n", fontsize);
    return;
  }

  /* see if fontname has changed */
  if (rotated_label_font[fontsize] &&
      strcmp(rotated_label_fontname[fontsize],current_rotated_label_fontname[fontsize]) != 0)
  {
    XFreeFont(XtDisplay(w),rotated_label_font[fontsize]);
    rotated_label_font[fontsize] = NULL;
    xastir_snprintf(current_rotated_label_fontname[fontsize],
                    sizeof(rotated_label_fontname),
                    "%s",
                    rotated_label_fontname[fontsize]);
  }
  /* load font */
  if(!rotated_label_font[fontsize])
  {
    rotated_label_font[fontsize]=(XFontStruct *)XLoadQueryFont(XtDisplay (w),
                                 rotated_label_fontname[fontsize]);
    if (rotated_label_font[fontsize] == NULL)      // Couldn't get the font!!!
    {
      fprintf(stderr,"draw_rotated_label_text: Couldn't get font %s\n",
              rotated_label_fontname[fontsize]);
      return;
    }
  }

  if (draw_outline)
  {
    // make outline style
    (void)XSetForeground(XtDisplay(w),gc,outline_bg_color);
    // Draw the string repeatedly with 1 pixel offsets in the
    // background color to make an outline.

    for (x_outline=-1; x_outline<2; x_outline++)
    {
      for (y_outline=-1; y_outline<2; y_outline++)
      {
        // draws one extra copy at x,y
        (void)XRotDrawAlignedString(XtDisplay (w),
                                    rotated_label_font[fontsize],
                                    my_rotation,
                                    target_pixmap,
                                    gc,
                                    x+x_outline,
                                    y+y_outline,
                                    label_text,
                                    align);
      }
    }
  }


  // Code to determine the bounding box corner points for the rotated text
//    corner = XRotTextExtents(w,rotated_label_font,my_rotation,x,y,label_text,BLEFT);
//    for (i=0;i<5;i++) {
//        fprintf(stderr,"%d,%d\t",corner[i].x,corner[i].y);
//    }
//    fprintf(stderr,"\n");

  (void)XSetForeground (XtDisplay (w), gc, color);

  //fprintf(stderr,"%0.1f\t%s\n",my_rotation,label_text);

  (void)XRotDrawAlignedString(XtDisplay (w),
                              rotated_label_font[fontsize],
                              my_rotation,
                              target_pixmap,
                              gc,
                              x,
                              y,
                              label_text,
                              align);
}





// Find the pixel length of an unrotated string in the rotated_label_font.
// Parameters:
//    w - the XtDisplay.
//    label_text - the string of which the length is to be found.
//    fontsize - the fontsize in the rotated_label_font in which the string
//    is to be rendered.
// Returns: the length in pixels of the string, -1 on an error.
int get_rotated_label_text_length_pixels(Widget w, char *label_text, int fontsize)
{
  int dir, asc, desc;   // parameters returned by XTextExtents, but not used here.
  XCharStruct overall;  // description of the space occupied by the string.
  int return_value;     // value to return
  int got_font;         // flag indicating that a font is available

  return_value = -1;
  got_font = TRUE;

  /* load font */
  if(!rotated_label_font[fontsize])
  {
    rotated_label_font[fontsize]=(XFontStruct *)XLoadQueryFont(XtDisplay (w),
                                 rotated_label_fontname[fontsize]);
    if (rotated_label_font[fontsize] == NULL)      // Couldn't get the font!!!
    {
      fprintf(stderr,"get_rotated_label_text_length_pixels: Couldn't get font %s\n",
              rotated_label_fontname[fontsize]);
      got_font = FALSE;
    }
  }

  if (got_font)
  {
    // find out the width in pixels of the unrotated label_text string.
    XTextExtents(rotated_label_font[fontsize], label_text, strlen(label_text), &dir, &asc, &desc,
                 &overall);
    return_value = overall.width;
  }

  return return_value;
}





// Find the pixel height of an unrotated string in the rotated_label_font.
// Parameters:
//    w - the XtDisplay.
//    label_text - the string of which the length is to be found.
//    fontsize - the fontsize in the rotated_label_font in which the string
//    is to be rendered.
// Returns: the height in pixels of the string, -1 on an error.
int get_rotated_label_text_height_pixels(Widget w, char *label_text, int fontsize)
{
  int dir, asc, desc;   // parameters returned by XTextExtents, but not used here.
  XCharStruct overall;  // description of the space occupied by the string.
  int return_value;     // value to return
  int got_font;         // flag indicating that a font is available

  return_value = -1;
  got_font = TRUE;

  /* load font */
  if(!rotated_label_font[fontsize])
  {
    rotated_label_font[fontsize]=(XFontStruct *)XLoadQueryFont(XtDisplay (w),
                                 rotated_label_fontname[fontsize]);
    if (rotated_label_font[fontsize] == NULL)      // Couldn't get the font!!!
    {
      fprintf(stderr,"get_rotated_label_text_height_pixels: Couldn't get font %s\n",
              rotated_label_fontname[fontsize]);
      got_font = FALSE;
    }
  }

  if (got_font)
  {
    // find out the width in pixels of the unrotated label_text string.
    XTextExtents(rotated_label_font[fontsize], label_text, strlen(label_text), &dir, &asc, &desc,
                 &overall);
    return_value = overall.ascent + overall.descent;
  }

  return return_value;
}





// Draw a rotated label onto the specified pixmap.
// Wrapper for draw_rotated_label_text-common().
void draw_rotated_label_text_to_target (Widget w, int rotation, int x, int y, int label_length, int color, char *label_text, int fontsize, Pixmap target_pixmap, int draw_outline, int outline_bg_color)
{
  float my_rotation = (float)((-rotation)-90);

  if ( ( (my_rotation < -90.0) && (my_rotation > -270.0) )
       || ( (my_rotation >  90.0) && (my_rotation <  270.0) ) )
  {
    my_rotation = my_rotation + 180.0;
    (void)draw_rotated_label_text_common(w,
                                         my_rotation,
                                         x,
                                         y,
                                         label_length,
                                         color,
                                         label_text,
                                         BRIGHT,
                                         fontsize,
                                         target_pixmap,
                                         draw_outline,
                                         outline_bg_color);
  }
  else
  {
    (void)draw_rotated_label_text_common(w,
                                         my_rotation,
                                         x,
                                         y,
                                         label_length,
                                         color,
                                         label_text,
                                         BLEFT,
                                         fontsize,
                                         target_pixmap,
                                         draw_outline,
                                         outline_bg_color);
  }
}





void draw_rotated_label_text (Widget w, int rotation, int x, int y, int label_length, int color, char *label_text, int fontsize)
{
  float my_rotation = (float)((-rotation)-90);

  if ( ( (my_rotation < -90.0) && (my_rotation > -270.0) )
       || ( (my_rotation >  90.0) && (my_rotation <  270.0) ) )
  {
    my_rotation = my_rotation + 180.0;
    (void)draw_rotated_label_text_common(w,
                                         my_rotation,
                                         x,
                                         y,
                                         label_length,
                                         color,
                                         label_text,
                                         BRIGHT,
                                         fontsize,
                                         pixmap, 0, 0);
  }
  else
  {
    (void)draw_rotated_label_text_common(w,
                                         my_rotation,
                                         x,
                                         y,
                                         label_length,
                                         color,
                                         label_text,
                                         BLEFT,
                                         fontsize,
                                         pixmap, 0, 0);
  }
}

void draw_centered_label_text (Widget w, int rotation, int x, int y, int label_length, int color, char *label_text, int fontsize)
{
  float my_rotation = (float)((-rotation)-90);

  (void)draw_rotated_label_text_common(w,
                                       my_rotation,
                                       x,
                                       y,
                                       label_length,
                                       color,
                                       label_text,
                                       BCENTRE,
                                       fontsize,
                                       pixmap, 0, 0);
}





static void Print_postscript_destroy_shell(Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  Widget shell = (Widget) clientData;
  char *temp_ptr;


  XtPopdown(shell);

  begin_critical_section(&print_postscript_dialog_lock, "maps.c:Print_postscript_destroy_shell" );

  if (print_postscript_dialog)
  {
    // Snag the path to the printer program from the print dialog
    temp_ptr = XmTextFieldGetString(printer_data);
    xastir_snprintf(printer_program,
                    sizeof(printer_program),
                    "%s",
                    temp_ptr);
    XtFree(temp_ptr);
    (void)remove_trailing_spaces(printer_program);

    // Check for empty variable
    if (printer_program[0] == '\0')
    {

#ifdef LPR_PATH
      // Path to LPR if defined
      xastir_snprintf(printer_program,
                      sizeof(printer_program),
                      "%s",
                      LPR_PATH);
#else // LPR_PATH
      // Empty path
      printer_program[0]='\0';
#endif // LPR_PATH
    }

//fprintf(stderr,"%s\n", printer_program);

    // Snag the path to the previewer program from the print dialog
    temp_ptr = XmTextFieldGetString(previewer_data);
    xastir_snprintf(previewer_program,
                    sizeof(previewer_program),
                    "%s",
                    temp_ptr);
    XtFree(temp_ptr);
    (void)remove_trailing_spaces(previewer_program);

    // Check for empty variable
    if (previewer_program[0] == '\0')
    {

#ifdef GV_PATH
      // Path to GV if defined
      xastir_snprintf(previewer_program,
                      sizeof(previewer_program),
                      "%s",
                      GV_PATH);
#else // GV_PATH
      // Empty string
      previewer_program[0] = '\0';
#endif // GV_PATH
    }
//fprintf(stderr,"%s\n", previewer_program);
  }

  XtDestroyWidget(shell);
  print_postscript_dialog = (Widget)NULL;

  end_critical_section(&print_postscript_dialog_lock, "maps.c:Print_postscript_destroy_shell" );

}





static void Print_properties_destroy_shell(Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  Widget shell = (Widget) clientData;

  if (!shell)
  {
    return;
  }

  XtPopdown(shell);

  begin_critical_section(&print_properties_dialog_lock, "maps.c:Print_properties_destroy_shell" );

  XtDestroyWidget(shell);
  print_properties_dialog = (Widget)NULL;

  end_critical_section(&print_properties_dialog_lock, "maps.c:Print_properties_destroy_shell" );

}





// Print_window:  Prints the drawing area to a Postscript file and
// then sends it to the printer program (usually "lpr).
//
static void Print_window( Widget widget, XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{

#ifdef NO_XPM
//    fprintf(stderr,"XPM or ImageMagick support not compiled into Xastir!\n");
  popup_message_always(langcode("POPEM00035"),
                       "XPM or ImageMagick support not compiled into Xastir! Cannot Print!");
#else   // NO_XPM

  char xpm_filename[MAX_FILENAME];
  char ps_filename[MAX_FILENAME];
  char command[MAX_FILENAME*2];
  char temp[MAX_FILENAME];
  int xpmretval;
  char temp_base_dir[MAX_VALUE];

  get_user_base_dir("tmp", temp_base_dir, sizeof(temp_base_dir));

  xastir_snprintf(xpm_filename,
                  sizeof(xpm_filename),
                  "%s/print.xpm",
                  temp_base_dir);

  xastir_snprintf(ps_filename,
                  sizeof(ps_filename),
                  "%s/print.ps",
                  temp_base_dir);

  busy_cursor(appshell);  // Show a busy cursor while we're doing all of this

  // Get rid of the Print dialog
  Print_postscript_destroy_shell(widget, print_postscript_dialog, NULL );

  if ( debug_level & 512 )
  {
    fprintf(stderr,"Creating %s\n", xpm_filename );
  }

  xastir_snprintf(temp, sizeof(temp), "%s", langcode("PRINT0012") );
  statusline(temp,1);       // Dumping image to file...

  if (chdir(temp_base_dir) != 0)
  {
    fprintf(stderr,"Couldn't chdir to %s directory for print_window\n", temp_base_dir);
    return;
  }

  xpmretval=XpmWriteFileFromPixmap(XtDisplay(appshell),// Display *display
                                   "print.xpm",                                 // char *filename
                                   pixmap_final,                                // Pixmap pixmap
                                   (Pixmap)NULL,                                // Pixmap shapemask
                                   NULL );

  if (xpmretval != XpmSuccess)
  {
    fprintf(stderr,"ERROR writing %s: %s\n", xpm_filename,
            XpmGetErrorString(xpmretval));
    popup_message_always(langcode("POPEM00035"),
                         "Error writing xpm image file! Cannot Print!");
    return;
  }
  else            // We now have the xpm file created on disk
  {

    chmod( xpm_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

    if ( debug_level & 512 )
    {
      fprintf(stderr,"Convert %s ==> %s\n", xpm_filename, ps_filename );
    }


    // Convert it to a postscript file for printing.  This depends
    // on the ImageMagick command "convert".
    //

    if (debug_level & 512)
    {
      fprintf(stderr,"Width: %ld\tHeight: %ld\n", screen_width, screen_height);
    }

    xastir_snprintf(temp, sizeof(temp), "%s", langcode("PRINT0013") );
    statusline(temp,1);       // Converting to Postscript...


#ifdef HAVE_CONVERT
    strcpy(command, CONVERT_PATH);
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, " -filter Point ");
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, xpm_filename);
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, " ");
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, ps_filename);
    command[sizeof(command)-1] = '\0';  // Terminate string

    if ( debug_level & 512 )
    {
      fprintf(stderr,"%s\n", command );
    }

    if ( system( command ) != 0 )
    {
//            fprintf(stderr,"\n\nPrint: Couldn't convert from XPM to PS!\n\n\n");
      popup_message_always(langcode("POPEM00035"),
                           "Couldn't convert from XPM to PS!");
      return;
    }
#endif  // HAVE_CONVERT

    chmod( ps_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

    // Delete temporary xpm file
    if ( !(debug_level & 512) )
    {
      unlink( xpm_filename );
    }

    if ( debug_level & 512 )
    {
      fprintf(stderr,"Printing postscript file %s\n", ps_filename);
    }

// Note: This needs to be changed to "lp" for Solaris.
// Also need to have a field to configure the printer name.  One
// fill-in field could do both.
//
// Since we could be running SUID root, we don't want to be
// calling "system" anyway.  Several problems with it.

    strcpy(command, printer_program);
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, " ");
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, ps_filename);
    command[sizeof(command)-1] = '\0';  // Terminate string

    if ( debug_level & 512 )
    {
      fprintf(stderr,"%s\n", command);
    }

    if (printer_program[0] == '\0')
    {
//            fprintf(stderr,"\n\nPrint: No print program defined!\n\n\n");
      popup_message_always(langcode("POPEM00035"),
                           "No print program defined!");
      return;
    }

    if ( system( command ) != 0 )
    {
//            fprintf(stderr,"\n\nPrint: Couldn't send to the printer!\n\n\n");
      popup_message_always(langcode("POPEM00035"),
                           "Couldn't send to the printer!");
      return;
    }

    /*
            if ( !(debug_level & 512) )
                unlink( ps_filename );
    */

    if ( debug_level & 512 )
    {
      fprintf(stderr,"  Done printing.\n");
    }
  }

  xastir_snprintf(temp, sizeof(temp), "%s", langcode("PRINT0014") );
  statusline(temp,1);       // Finished creating print file.

  //popup_message( langcode("PRINT0015"), langcode("PRINT0014") );

#endif // NO_XPM

}





// Print_preview:  Prints the drawing area to a Postscript file.  If
// previewer_program has "gv" in it, then use the various options
// selected by the user.  If not, skip those options.
//
static void Print_preview( Widget widget, XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{

#ifdef NO_XPM
//    fprintf(stderr,"XPM or ImageMagick support not compiled into Xastir!\n");
  popup_message_always(langcode("POPEM00035"),
                       "XPM or ImageMagick support not compiled into Xastir! Cannot Print!");
#else   // NO_GRAPHICS

  char xpm_filename[MAX_FILENAME];
  char ps_filename[MAX_FILENAME];
  char mono[50] = "";
  char invert[50] = "";
  char rotate[50] = "";
  char scale[50] = "";
  char density[50] = "";
  char command[MAX_FILENAME*2];
  char temp[MAX_FILENAME];
  char format[100] = " ";
  int xpmretval;
  char temp_base_dir[MAX_VALUE];

  get_user_base_dir("tmp", temp_base_dir, sizeof(temp_base_dir));


  xastir_snprintf(xpm_filename,
                  sizeof(xpm_filename),
                  "%s/print.xpm",
                  temp_base_dir);

  xastir_snprintf(ps_filename,
                  sizeof(ps_filename),
                  "%s/print.ps",
                  temp_base_dir);

  busy_cursor(appshell);  // Show a busy cursor while we're doing all of this

  // Get rid of the Print Properties dialog if it exists
  Print_properties_destroy_shell(widget, print_properties_dialog, NULL );

  if ( debug_level & 512 )
  {
    fprintf(stderr,"Creating %s\n", xpm_filename );
  }

  xastir_snprintf(temp, sizeof(temp), "%s", langcode("PRINT0012") );
  statusline(temp,1);       // Dumping image to file...

  if (chdir(temp_base_dir) != 0)
  {
    fprintf(stderr,"Couldn't chdir to %s directory for print_preview\n", temp_base_dir);
    return;
  }

  xpmretval=XpmWriteFileFromPixmap(XtDisplay(appshell),// Display *display
                                   "print.xpm",                                 // char *filename
                                   pixmap_final,                                // Pixmap pixmap
                                   (Pixmap)NULL,                                // Pixmap shapemask
                                   NULL );

  if (xpmretval != XpmSuccess)
  {
    fprintf(stderr,"ERROR writing %s: %s\n", xpm_filename,
            XpmGetErrorString(xpmretval));
    popup_message_always(langcode("POPEM00035"),
                         "Error writing XPM file!");
    return;
  }
  else            // We now have the xpm file created on disk
  {

    chmod( xpm_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

    if ( debug_level & 512 )
    {
      fprintf(stderr,"Convert %s ==> %s\n", xpm_filename, ps_filename );
    }


    // If we're not using "gv", skip most of the code below and
    // go straight to the previewer program portion of the code.
    //
    if ( strstr(previewer_program,"gv") )
    {

      // Convert it to a postscript file for printing.  This
      // depends on the ImageMagick command "convert".
      //
      // Other options to try in the future:
      // -label
      //
      if ( print_auto_scale )
      {
//                sprintf(scale, "-geometry 612x792 -page 612x792 ");   // "Letter" size at 72 dpi
//                sprintf(scale, "-sample 612x792 -page 612x792 ");     // "Letter" size at 72 dpi
        xastir_snprintf(scale, sizeof(scale), "-page 1275x1650+0+0 "); // "Letter" size at 150 dpi
      }
      else
      {
        scale[0] = '\0';  // Empty string
      }


      if ( print_in_monochrome )
      {
        xastir_snprintf(mono, sizeof(mono), "-monochrome +dither " );  // Monochrome
      }
      else
      {
        xastir_snprintf(mono, sizeof(mono), "+dither ");  // Color
      }


      if ( print_invert )
      {
        xastir_snprintf(invert, sizeof(invert), "-negate " );  // Reverse Colors
      }
      else
      {
        invert[0] = '\0';  // Empty string
      }


      if (debug_level & 512)
      {
        fprintf(stderr,"Width: %ld\tHeight: %ld\n", screen_width, screen_height);
      }


      if ( print_rotated )
      {
        xastir_snprintf(rotate, sizeof(rotate), "-rotate -90 " );

#ifdef HAVE_OLD_GV
        xastir_snprintf(format, sizeof(format), "-landscape " );
#else   // HAVE_OLD_GV
        xastir_snprintf(format, sizeof(format), "--orientation=landscape " );
#endif  // HAVE_OLD_GV

      }
      else if ( print_auto_rotation )
      {
        // Check whether the width or the height of the
        // pixmap is greater.  If width is greater than
        // height, rotate the image by 270 degrees.
        if (screen_width > screen_height)
        {
          xastir_snprintf(rotate, sizeof(rotate), "-rotate -90 " );

#ifdef HAVE_OLD_GV
          xastir_snprintf(format, sizeof(format), "-landscape " );
#else   // HAVE_OLD_GV
          xastir_snprintf(format, sizeof(format), "--orientation=landscape " );
#endif  // HAVE_OLD_GV

          if (debug_level & 512)
          {
            fprintf(stderr,"Rotating\n");
          }
        }
        else
        {
          rotate[0] = '\0';   // Empty string
          if (debug_level & 512)
          {
            fprintf(stderr,"Not Rotating\n");
          }
        }
      }
      else
      {
        rotate[0] = '\0';   // Empty string
        if (debug_level & 512)
        {
          fprintf(stderr,"Not Rotating\n");
        }
      }


      // Higher print densities require more memory and time
      // to process
      xastir_snprintf(density, sizeof(density), "-density %dx%d", print_resolution,
                      print_resolution );

      xastir_snprintf(temp, sizeof(temp), "%s", langcode("PRINT0013") );
      statusline(temp,1);       // Converting to Postscript...


      // Filters:
      // Point (ok at higher dpi's)
      // Box  (not too bad)
      // Triangle (no)
      // Hermite (no)
      // Hanning (no)
      // Hamming (no)
      // Blackman (better but still not good)
      // Gaussian (no)
      // Quadratic (no)
      // Cubic (no)
      // Catrom (not too bad)
      // Mitchell (no)
      // Lanczos (no)
      // Bessel (no)
      // Sinc (not too bad)

    }

#ifdef HAVE_CONVERT
    strcpy(command, CONVERT_PATH);
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, " -filter Point ");
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, mono);
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, invert);
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, rotate);
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, scale);
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, density);
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, " ");
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, xpm_filename);
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, " ");
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, ps_filename);
    command[sizeof(command)-1] = '\0';  // Terminate string

    if ( debug_level & 512 )
    {
      fprintf(stderr,"%s\n", command );
    }

    if ( system( command ) != 0 )
    {
//            fprintf(stderr,"\n\nPrint: Couldn't convert from XPM to PS!\n\n\n");
      popup_message_always(langcode("POPEM00035"),
                           "Couldn't convert from XPM to PS!");
      return;
    }
#endif  // HAVE_CONVERT

    chmod( ps_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

    // Delete temporary xpm file
    if ( !(debug_level & 512) )
    {
      unlink( xpm_filename );
    }

    if ( debug_level & 512 )
    {
      fprintf(stderr,"Printing postscript file %s\n", ps_filename);
    }

// Since we could be running SUID root, we don't want to be
// calling "system" anyway.  Several problems with it.

    // Bring up the postscript viewer
    strcpy(command, previewer_program);
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, " ");
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, format);
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, " ");
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, ps_filename);
    command[sizeof(command)-1] = '\0';  // Terminate string
    strcat(command, " &");
    command[sizeof(command)-1] = '\0';  // Terminate string

    if ( debug_level & 512 )
    {
      fprintf(stderr,"%s\n", command);
    }

    if (previewer_program[0] == '\0')
    {
//            fprintf(stderr,"\n\nPrint: No print previewer defined!\n\n\n");
      popup_message_always(langcode("POPEM00035"),
                           "No print previewer defined!");
      return;
    }

    if ( system( command ) != 0 )
    {
//            fprintf(stderr,"\n\nPrint: Couldn't bring up the postscript viewer!\n\n\n");
      popup_message_always(langcode("POPEM00035"),
                           "Couldn't bring up the viewer!");
      return;
    }

    /*
            if ( !(debug_level & 512) )
                unlink( ps_filename );
    */

    if ( debug_level & 512 )
    {
      fprintf(stderr,"  Done printing.\n");
    }
  }

  xastir_snprintf(temp, sizeof(temp), "%s", langcode("PRINT0014") );
  statusline(temp,1);       // Finished creating print file.

  //popup_message( langcode("PRINT0015"), langcode("PRINT0014") );

#endif // NO_XPM

}





/*
 *  Auto_rotate
 *
 */
static void  Auto_rotate( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    print_auto_rotation = atoi(which);
    print_rotated = 0;
    XmToggleButtonSetState(rotate_90, FALSE, FALSE);
  }
  else
  {
    print_auto_rotation = 0;
  }
}





/*
 *  Rotate_90
 *
 */
static void  Rotate_90( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    print_rotated = atoi(which);
    print_auto_rotation = 0;
    XmToggleButtonSetState(auto_rotate, FALSE, FALSE);
  }
  else
  {
    print_rotated = 0;
  }
}





/*
 *  Auto_scale
 *
 */
static void  Auto_scale( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    print_auto_scale = atoi(which);
  }
  else
  {
    print_auto_scale = 0;
  }
}





/*
 *  Monochrome
 *
 */
void  Monochrome( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    print_in_monochrome = atoi(which);
  }
  else
  {
    print_in_monochrome = 0;
  }
}





/*
 *  Invert
 *
 */
static void  Invert( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    print_invert = atoi(which);
  }
  else
  {
    print_invert = 0;
  }
}





// Print_properties:  Prints the drawing area to a PostScript file.
// Provides various togglebuttons for configuring the "gv" previewer
// only.
//
// Perhaps later:
// 1) Select an area on the screen to print
// 2) -label
//
void Print_properties( Widget w, XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  static Widget pane, form, button_ok, button_cancel,
         sep, auto_scale,
//            paper_size, paper_size_data, scale, scale_data, blank_background,
//            res_label1, res_label2, res_x, res_y,
         monochrome, invert;
  Atom delw;

  // Get rid of the Print dialog
  Print_postscript_destroy_shell(w, print_postscript_dialog, NULL );


  // If we're not using "gv", skip the entire dialog below and go
  // straight to the actual previewer function.
  //
  if ( !strstr(previewer_program,"gv") )
  {
    Print_preview(w, NULL, NULL);
    return;
  }


  if (!print_properties_dialog)
  {


    begin_critical_section(&print_properties_dialog_lock, "maps.c:Print_properties" );


    print_properties_dialog = XtVaCreatePopupShell(langcode("PRINT0001"),
                              xmDialogShellWidgetClass, appshell,
                              XmNdeleteResponse, XmDESTROY,
                              XmNdefaultPosition, FALSE,
                              XmNfontList, fontlist1,
                              NULL);


    pane = XtVaCreateWidget("Print_properties pane",xmPanedWindowWidgetClass, print_properties_dialog,
                            XmNbackground, colors[0xff],
                            NULL);


    form =  XtVaCreateWidget("Print_properties form",xmFormWidgetClass, pane,
                             XmNfractionBase, 2,
                             XmNbackground, colors[0xff],
                             XmNautoUnmanage, FALSE,
                             XmNshadowThickness, 1,
                             NULL);


    /*
            paper_size = XtVaCreateManagedWidget(langcode("PRINT0002"),xmLabelWidgetClass, form,
                                          XmNtopAttachment, XmATTACH_FORM,
                                          XmNtopOffset, 10,
                                          XmNbottomAttachment, XmATTACH_NONE,
                                          XmNleftAttachment, XmATTACH_FORM,
                                          XmNleftOffset, 10,
                                          XmNrightAttachment, XmATTACH_NONE,
                                          XmNbackground, colors[0xff],
                                          XmNfontList, fontlist1,
                                          NULL);
    XtSetSensitive(paper_size,FALSE);


            paper_size_data = XtVaCreateManagedWidget("Print_properties paper_size_data", xmTextFieldWidgetClass, form,
                                          XmNeditable,   TRUE,
                                          XmNcursorPositionVisible, TRUE,
                                          XmNsensitive, TRUE,
                                          XmNshadowThickness,    1,
                                          XmNcolumns, 15,
                                          XmNwidth, ((15*7)+2),
                                          XmNmaxLength, 15,
                                          XmNbackground, colors[0x0f],
                                          XmNtopAttachment,XmATTACH_FORM,
                                          XmNtopOffset, 5,
                                          XmNbottomAttachment,XmATTACH_NONE,
                                          XmNleftAttachment, XmATTACH_WIDGET,
                                          XmNleftWidget, paper_size,
                                          XmNleftOffset, 10,
                                          XmNrightAttachment,XmATTACH_FORM,
                                          XmNrightOffset, 10,
                                          XmNnavigationType, XmTAB_GROUP,
                                          XmNtraversalOn, TRUE,
                                          XmNfontList, fontlist1,
                                          NULL);
    XtSetSensitive(paper_size_data,FALSE);
    */


    auto_rotate  = XtVaCreateManagedWidget(langcode("PRINT0003"),xmToggleButtonWidgetClass,form,
//                                      XmNtopAttachment, XmATTACH_WIDGET,
//                                      XmNtopWidget, paper_size_data,
//                                      XmNtopOffset, 5,
                                           XmNtopAttachment, XmATTACH_FORM,
                                           XmNtopOffset, 10,
                                           XmNbottomAttachment, XmATTACH_NONE,
                                           XmNleftAttachment, XmATTACH_FORM,
                                           XmNleftOffset,10,
                                           XmNrightAttachment, XmATTACH_NONE,
                                           XmNbackground, colors[0xff],
                                           XmNnavigationType, XmTAB_GROUP,
                                           XmNtraversalOn, TRUE,
                                           XmNfontList, fontlist1,
                                           NULL);
    XtAddCallback(auto_rotate,XmNvalueChangedCallback,Auto_rotate,"1");


    rotate_90  = XtVaCreateManagedWidget(langcode("PRINT0004"),xmToggleButtonWidgetClass,form,
//                                      XmNtopAttachment, XmATTACH_WIDGET,
//                                      XmNtopWidget, paper_size_data,
//                                      XmNtopOffset, 5,
                                         XmNtopAttachment, XmATTACH_FORM,
                                         XmNtopOffset, 10,
                                         XmNbottomAttachment, XmATTACH_NONE,
                                         XmNleftAttachment, XmATTACH_WIDGET,
                                         XmNleftWidget, auto_rotate,
                                         XmNleftOffset,10,
                                         XmNrightAttachment, XmATTACH_FORM,
                                         XmNrightOffset, 10,
                                         XmNbackground, colors[0xff],
                                         XmNnavigationType, XmTAB_GROUP,
                                         XmNtraversalOn, TRUE,
                                         XmNfontList, fontlist1,
                                         NULL);
    XtAddCallback(rotate_90,XmNvalueChangedCallback,Rotate_90,"1");


    auto_scale = XtVaCreateManagedWidget(langcode("PRINT0005"),xmToggleButtonWidgetClass,form,
                                         XmNtopAttachment, XmATTACH_WIDGET,
                                         XmNtopWidget, auto_rotate,
                                         XmNtopOffset, 5,
                                         XmNbottomAttachment, XmATTACH_NONE,
                                         XmNleftAttachment, XmATTACH_FORM,
                                         XmNleftOffset,10,
                                         XmNrightAttachment, XmATTACH_NONE,
                                         XmNbackground, colors[0xff],
                                         XmNnavigationType, XmTAB_GROUP,
                                         XmNtraversalOn, TRUE,
                                         XmNfontList, fontlist1,
                                         NULL);
    XtAddCallback(auto_scale,XmNvalueChangedCallback,Auto_scale,"1");


    /*
            scale = XtVaCreateManagedWidget(langcode("PRINT0006"),xmLabelWidgetClass, form,
                                          XmNtopAttachment, XmATTACH_WIDGET,
                                          XmNtopWidget, auto_rotate,
                                          XmNtopOffset, 10,
                                          XmNbottomAttachment, XmATTACH_NONE,
                                          XmNleftAttachment, XmATTACH_WIDGET,
                                          XmNleftWidget, auto_scale,
                                          XmNleftOffset, 10,
                                          XmNrightAttachment, XmATTACH_NONE,
                                          XmNbackground, colors[0xff],
                                          XmNfontList, fontlist1,
                                          NULL);
    XtSetSensitive(scale,FALSE);


            scale_data = XtVaCreateManagedWidget("Print_properties scale_data", xmTextFieldWidgetClass, form,
                                          XmNeditable,   TRUE,
                                          XmNcursorPositionVisible, TRUE,
                                          XmNsensitive, TRUE,
                                          XmNshadowThickness,    1,
                                          XmNcolumns, 15,
                                          XmNwidth, ((15*7)+2),
                                          XmNmaxLength, 15,
                                          XmNbackground, colors[0x0f],
                                          XmNtopAttachment,XmATTACH_WIDGET,
                                          XmNtopWidget, auto_rotate,
                                          XmNtopOffset, 5,
                                          XmNbottomAttachment,XmATTACH_NONE,
                                          XmNleftAttachment, XmATTACH_WIDGET,
                                          XmNleftWidget, scale,
                                          XmNleftOffset, 10,
                                          XmNrightAttachment,XmATTACH_FORM,
                                          XmNrightOffset, 10,
                                          XmNnavigationType, XmTAB_GROUP,
                                          XmNtraversalOn, TRUE,
                                          XmNfontList, fontlist1,
                                          NULL);
    XtSetSensitive(scale_data,FALSE);
    */


    /*
            blank_background = XtVaCreateManagedWidget(langcode("PRINT0007"),xmToggleButtonWidgetClass,form,
                                          XmNtopAttachment, XmATTACH_WIDGET,
                                          XmNtopWidget, scale_data,
                                          XmNtopWidget, auto_rotate,
                                          XmNtopOffset, 5,
                                          XmNbottomAttachment, XmATTACH_NONE,
                                          XmNleftAttachment, XmATTACH_FORM,
                                          XmNleftOffset ,10,
                                          XmNrightAttachment, XmATTACH_NONE,
                                          XmNbackground, colors[0xff],
                                          XmNnavigationType, XmTAB_GROUP,
                                          XmNtraversalOn, TRUE,
                                          XmNfontList, fontlist1,
                                          NULL);
    XtSetSensitive(blank_background,FALSE);
    */


    monochrome = XtVaCreateManagedWidget(langcode("PRINT0008"),xmToggleButtonWidgetClass,form,
                                         XmNtopAttachment, XmATTACH_WIDGET,
//                                      XmNtopWidget, blank_background,
                                         XmNtopWidget, auto_scale,
                                         XmNtopOffset, 5,
                                         XmNbottomAttachment, XmATTACH_NONE,
                                         XmNleftAttachment, XmATTACH_FORM,
                                         XmNleftOffset,10,
                                         XmNrightAttachment, XmATTACH_NONE,
                                         XmNbackground, colors[0xff],
                                         XmNnavigationType, XmTAB_GROUP,
                                         XmNtraversalOn, TRUE,
                                         XmNfontList, fontlist1,
                                         NULL);
    XtAddCallback(monochrome,XmNvalueChangedCallback,Monochrome,"1");


    invert = XtVaCreateManagedWidget(langcode("PRINT0016"),xmToggleButtonWidgetClass,form,
                                     XmNtopAttachment, XmATTACH_WIDGET,
                                     XmNtopWidget, monochrome,
                                     XmNtopOffset, 5,
                                     XmNbottomAttachment, XmATTACH_NONE,
                                     XmNleftAttachment, XmATTACH_FORM,
                                     XmNleftOffset,10,
                                     XmNrightAttachment, XmATTACH_NONE,
                                     XmNbackground, colors[0xff],
                                     XmNnavigationType, XmTAB_GROUP,
                                     XmNtraversalOn, TRUE,
                                     XmNfontList, fontlist1,
                                     NULL);
    XtAddCallback(invert,XmNvalueChangedCallback,Invert,"1");


    /*
            res_label1 = XtVaCreateManagedWidget(langcode("PRINT0009"),xmLabelWidgetClass, form,
                                          XmNtopAttachment, XmATTACH_WIDGET,
                                          XmNtopWidget, invert,
                                          XmNtopOffset, 10,
                                          XmNbottomAttachment, XmATTACH_NONE,
                                          XmNleftAttachment, XmATTACH_FORM,
                                          XmNleftOffset, 10,
                                          XmNrightAttachment, XmATTACH_NONE,
                                          XmNbackground, colors[0xff],
                                          XmNfontList, fontlist1,
                                          NULL);
    XtSetSensitive(res_label1,FALSE);


            res_x = XtVaCreateManagedWidget("Print_properties resx_data", xmTextFieldWidgetClass, form,
                                          XmNeditable,   TRUE,
                                          XmNcursorPositionVisible, TRUE,
                                          XmNsensitive, TRUE,
                                          XmNshadowThickness,    1,
                                          XmNcolumns, 15,
                                          XmNwidth, ((15*7)+2),
                                          XmNmaxLength, 15,
                                          XmNbackground, colors[0x0f],
                                          XmNtopAttachment,XmATTACH_WIDGET,
                                          XmNtopWidget, invert,
                                          XmNtopOffset, 5,
                                          XmNbottomAttachment,XmATTACH_NONE,
                                          XmNleftAttachment, XmATTACH_WIDGET,
                                          XmNleftWidget, res_label1,
                                          XmNleftOffset, 10,
                                          XmNrightAttachment,XmATTACH_NONE,
                                          XmNnavigationType, XmTAB_GROUP,
                                          XmNtraversalOn, TRUE,
                                          XmNfontList, fontlist1,
                                          NULL);
    XtSetSensitive(res_x,FALSE);


            res_label2 = XtVaCreateManagedWidget("X",xmLabelWidgetClass, form,
                                          XmNtopAttachment, XmATTACH_WIDGET,
                                          XmNtopWidget, invert,
                                          XmNtopOffset, 10,
                                          XmNbottomAttachment, XmATTACH_NONE,
                                          XmNleftAttachment, XmATTACH_WIDGET,
                                          XmNleftWidget, res_x,
                                          XmNleftOffset, 10,
                                          XmNrightAttachment, XmATTACH_NONE,
                                          XmNbackground, colors[0xff],
                                          XmNfontList, fontlist1,
                                          NULL);
    XtSetSensitive(res_label2,FALSE);


            res_y = XtVaCreateManagedWidget("Print_properties res_y_data", xmTextFieldWidgetClass, form,
                                          XmNeditable,   TRUE,
                                          XmNcursorPositionVisible, TRUE,
                                          XmNsensitive, TRUE,
                                          XmNshadowThickness,    1,
                                          XmNcolumns, 15,
                                          XmNwidth, ((15*7)+2),
                                          XmNmaxLength, 15,
                                          XmNbackground, colors[0x0f],
                                          XmNtopAttachment,XmATTACH_WIDGET,
                                          XmNtopWidget, invert,
                                          XmNtopOffset, 5,
                                          XmNbottomAttachment,XmATTACH_NONE,
                                          XmNleftAttachment, XmATTACH_WIDGET,
                                          XmNleftWidget, res_label2,
                                          XmNleftOffset, 10,
                                          XmNrightAttachment,XmATTACH_FORM,
                                          XmNrightOffset, 10,
                                          XmNnavigationType, XmTAB_GROUP,
                                          XmNtraversalOn, TRUE,
                                          XmNfontList, fontlist1,
                                          NULL);
    XtSetSensitive(res_y,FALSE);
    */


    sep = XtVaCreateManagedWidget("Print_properties sep", xmSeparatorGadgetClass,form,
                                  XmNorientation, XmHORIZONTAL,
                                  XmNtopAttachment,XmATTACH_WIDGET,
//                                      XmNtopWidget, res_y,
                                  XmNtopWidget, invert,
                                  XmNtopOffset, 10,
                                  XmNbottomAttachment,XmATTACH_NONE,
                                  XmNleftAttachment, XmATTACH_FORM,
                                  XmNrightAttachment,XmATTACH_FORM,
                                  XmNbackground, colors[0xff],
                                  XmNfontList, fontlist1,
                                  NULL);


//        button_ok = XtVaCreateManagedWidget(langcode("PRINT0011"),xmPushButtonGadgetClass, form,
    button_ok = XtVaCreateManagedWidget(langcode("PRINT0010"),xmPushButtonGadgetClass, form,
                                        XmNtopAttachment, XmATTACH_WIDGET,
                                        XmNtopWidget, sep,
                                        XmNtopOffset, 5,
                                        XmNbottomAttachment, XmATTACH_FORM,
                                        XmNbottomOffset, 5,
                                        XmNleftAttachment, XmATTACH_POSITION,
                                        XmNleftPosition, 0,
                                        XmNleftOffset, 3,
                                        XmNrightAttachment, XmATTACH_POSITION,
                                        XmNrightPosition, 1,
                                        XmNrightOffset, 2,
                                        XmNbackground, colors[0xff],
                                        XmNnavigationType, XmTAB_GROUP,
                                        XmNtraversalOn, TRUE,
                                        XmNfontList, fontlist1,
                                        NULL);


    button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, form,
                                            XmNtopAttachment, XmATTACH_WIDGET,
                                            XmNtopWidget, sep,
                                            XmNtopOffset, 5,
                                            XmNbottomAttachment, XmATTACH_FORM,
                                            XmNbottomOffset, 5,
                                            XmNleftAttachment, XmATTACH_POSITION,
                                            XmNleftPosition, 1,
                                            XmNleftOffset, 3,
                                            XmNrightAttachment, XmATTACH_POSITION,
                                            XmNrightPosition, 2,
                                            XmNrightOffset, 5,
                                            XmNbackground, colors[0xff],
                                            XmNnavigationType, XmTAB_GROUP,
                                            XmNtraversalOn, TRUE,
                                            XmNfontList, fontlist1,
                                            NULL);


    XtAddCallback(button_ok, XmNactivateCallback, Print_preview, NULL );
    XtAddCallback(button_cancel, XmNactivateCallback, Print_properties_destroy_shell, print_properties_dialog);


    XmToggleButtonSetState(rotate_90,FALSE,FALSE);
    XmToggleButtonSetState(auto_rotate,TRUE,FALSE);


    if (print_auto_rotation)
    {
      XmToggleButtonSetState(auto_rotate, TRUE, TRUE);
    }
    else
    {
      XmToggleButtonSetState(auto_rotate, FALSE, TRUE);
    }


    if (print_rotated)
    {
      XmToggleButtonSetState(rotate_90, TRUE, TRUE);
    }
    else
    {
      XmToggleButtonSetState(rotate_90, FALSE, TRUE);
    }


    if (print_in_monochrome)
    {
      XmToggleButtonSetState(monochrome, TRUE, FALSE);
    }
    else
    {
      XmToggleButtonSetState(monochrome, FALSE, FALSE);
    }


    if (print_invert)
    {
      XmToggleButtonSetState(invert, TRUE, FALSE);
    }
    else
    {
      XmToggleButtonSetState(invert, FALSE, FALSE);
    }


    if (print_auto_scale)
    {
      XmToggleButtonSetState(auto_scale, TRUE, TRUE);
    }
    else
    {
      XmToggleButtonSetState(auto_scale, FALSE, TRUE);
    }


//        XmTextFieldSetString(paper_size_data,print_paper_size);


    end_critical_section(&print_properties_dialog_lock, "maps.c:Print_properties" );


    pos_dialog(print_properties_dialog);


    delw = XmInternAtom(XtDisplay(print_properties_dialog),"WM_DELETE_WINDOW", FALSE);
    XmAddWMProtocolCallback(print_properties_dialog, delw, Print_properties_destroy_shell, (XtPointer)print_properties_dialog);


    XtManageChild(form);
    XtManageChild(pane);


    XtPopup(print_properties_dialog,XtGrabNone);
    fix_dialog_size(print_properties_dialog);


    // Move focus to the Cancel button.  This appears to highlight the
    // button fine, but we're not able to hit the <Enter> key to
    // have that default function happen.  Note:  We _can_ hit the
    // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(print_properties_dialog);
    XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);


  }
  else
  {
    (void)XRaiseWindow(XtDisplay(print_properties_dialog), XtWindow(print_properties_dialog));
  }
}





// General print dialog.  From here we can either print Postscript
// files to the device selected in this dialog, or head off to a
// print preview program that might allow us a variety of print
// options.  From here we should be able to set the print device
// and the print preview program & path.
//
void Print_Postscript( Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  static Widget pane, form, button_print, button_cancel,
         sep, button_preview;
  Atom delw;

  if (!print_postscript_dialog)
  {


    begin_critical_section(&print_postscript_dialog_lock, "maps.c:Print_Postscript" );


    print_postscript_dialog = XtVaCreatePopupShell(langcode("PULDNFI015"),
                              xmDialogShellWidgetClass, appshell,
                              XmNdeleteResponse, XmDESTROY,
                              XmNdefaultPosition, FALSE,
                              XmNfontList, fontlist1,
                              NULL);


    pane = XtVaCreateWidget("Print_postscript pane",xmPanedWindowWidgetClass, print_postscript_dialog,
                            XmNbackground, colors[0xff],
                            NULL);


    form =  XtVaCreateWidget("Print_postscript form",xmFormWidgetClass, pane,
                             XmNfractionBase, 3,
                             XmNbackground, colors[0xff],
                             XmNautoUnmanage, FALSE,
                             XmNshadowThickness, 1,
                             NULL);


    // "Direct to:"
    button_print = XtVaCreateManagedWidget(langcode("PRINT1001"),xmPushButtonGadgetClass, form,
                                           XmNtopAttachment, XmATTACH_FORM,
                                           XmNtopOffset, 5,
                                           XmNbottomAttachment, XmATTACH_NONE,
                                           XmNleftAttachment, XmATTACH_FORM,
                                           XmNleftOffset, 5,
                                           XmNrightAttachment, XmATTACH_NONE,
                                           XmNbackground, colors[0xff],
                                           XmNnavigationType, XmTAB_GROUP,
                                           XmNtraversalOn, TRUE,
                                           XmNfontList, fontlist1,
                                           NULL);


    printer_data = XtVaCreateManagedWidget("Print_Postscript printer_data", xmTextFieldWidgetClass, form,
                                           XmNeditable,   TRUE,
                                           XmNcursorPositionVisible, TRUE,
                                           XmNsensitive, TRUE,
                                           XmNshadowThickness,    1,
                                           XmNcolumns, 40,
                                           XmNwidth, ((40*7)+2),
                                           XmNmaxLength, MAX_FILENAME,
                                           XmNbackground, colors[0x0f],
                                           XmNtopAttachment,XmATTACH_FORM,
                                           XmNtopOffset, 5,
                                           XmNbottomAttachment,XmATTACH_NONE,
                                           XmNleftAttachment, XmATTACH_WIDGET,
                                           XmNleftWidget, button_print,
                                           XmNleftOffset, 10,
                                           XmNrightAttachment,XmATTACH_FORM,
                                           XmNrightOffset, 5,
                                           XmNnavigationType, XmTAB_GROUP,
                                           XmNtraversalOn, TRUE,
                                           XmNfontList, fontlist1,
                                           NULL);


    // "Via Previewer:"
    button_preview = XtVaCreateManagedWidget(langcode("PRINT1002"),xmPushButtonGadgetClass, form,
                     XmNtopAttachment, XmATTACH_WIDGET,
                     XmNtopWidget, button_print,
                     XmNtopOffset, 5,
                     XmNbottomAttachment, XmATTACH_NONE,
                     XmNleftAttachment, XmATTACH_FORM,
                     XmNleftOffset, 5,
                     XmNrightAttachment, XmATTACH_NONE,
                     XmNbackground, colors[0xff],
                     XmNnavigationType, XmTAB_GROUP,
                     XmNtraversalOn, TRUE,
                     XmNfontList, fontlist1,
                     NULL);


    previewer_data = XtVaCreateManagedWidget("Print_Postscript previewer_data", xmTextFieldWidgetClass, form,
                     XmNeditable,   TRUE,
                     XmNcursorPositionVisible, TRUE,
                     XmNsensitive, TRUE,
                     XmNshadowThickness,    1,
                     XmNcolumns, 40,
                     XmNwidth, ((40*7)+2),
                     XmNmaxLength, MAX_FILENAME,
                     XmNbackground, colors[0x0f],
                     XmNtopAttachment,XmATTACH_WIDGET,
                     XmNtopWidget, button_print,
                     XmNtopOffset, 5,
                     XmNbottomAttachment,XmATTACH_NONE,
                     XmNleftAttachment, XmATTACH_WIDGET,
                     XmNleftWidget, button_preview,
                     XmNleftOffset, 10,
                     XmNrightAttachment,XmATTACH_FORM,
                     XmNrightOffset, 5,
                     XmNnavigationType, XmTAB_GROUP,
                     XmNtraversalOn, TRUE,
                     XmNfontList, fontlist1,
                     NULL);


    sep = XtVaCreateManagedWidget("Print_postscript sep", xmSeparatorGadgetClass,form,
                                  XmNorientation, XmHORIZONTAL,
                                  XmNtopAttachment,XmATTACH_WIDGET,
                                  XmNtopWidget, button_preview,
                                  XmNtopOffset, 10,
                                  XmNbottomAttachment,XmATTACH_NONE,
                                  XmNleftAttachment, XmATTACH_FORM,
                                  XmNrightAttachment,XmATTACH_FORM,
                                  XmNbackground, colors[0xff],
                                  XmNfontList, fontlist1,
                                  NULL);


    button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, form,
                                            XmNtopAttachment, XmATTACH_WIDGET,
                                            XmNtopWidget, sep,
                                            XmNtopOffset, 5,
                                            XmNbottomAttachment, XmATTACH_FORM,
                                            XmNbottomOffset, 5,
                                            XmNleftAttachment, XmATTACH_FORM,
                                            XmNleftOffset, 5,
                                            XmNrightAttachment, XmATTACH_FORM,
                                            XmNrightOffset, 5,
                                            XmNbackground, colors[0xff],
                                            XmNnavigationType, XmTAB_GROUP,
                                            XmNtraversalOn, TRUE,
                                            XmNfontList, fontlist1,
                                            NULL);


    XtAddCallback(button_preview, XmNactivateCallback, Print_properties, NULL );
    XtAddCallback(button_print, XmNactivateCallback, Print_window, NULL );
    XtAddCallback(button_cancel, XmNactivateCallback, Print_postscript_destroy_shell, print_postscript_dialog);

    // Fill in the text fields from persistent variables out of the config file.
    XmTextFieldSetString(printer_data, printer_program);
    XmTextFieldSetString(previewer_data, previewer_program);

    end_critical_section(&print_postscript_dialog_lock, "maps.c:Print_Postscript" );


    pos_dialog(print_postscript_dialog);


    delw = XmInternAtom(XtDisplay(print_postscript_dialog),"WM_DELETE_WINDOW", FALSE);
    XmAddWMProtocolCallback(print_postscript_dialog, delw, Print_postscript_destroy_shell, (XtPointer)print_postscript_dialog);


    XtManageChild(form);
    XtManageChild(pane);


    XtPopup(print_postscript_dialog,XtGrabNone);
    fix_dialog_size(print_postscript_dialog);


    // Move focus to the Cancel button.  This appears to highlight the
    // button fine, but we're not able to hit the <Enter> key to
    // have that default function happen.  Note:  We _can_ hit the
    // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(print_postscript_dialog);
    XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);


  }
  else
  {
    (void)XRaiseWindow(XtDisplay(print_postscript_dialog), XtWindow(print_postscript_dialog));
  }
}





// Create png image (for use in web browsers??).  Requires that "convert"
// from the ImageMagick package be installed on the system.  At the
// point this thread is started, the XPM file has already been
// created.  We now create a .geo file to go with the .png file.
//
#ifndef NO_XPM
static void* snapshot_thread(void * UNUSED(arg) )
{
  char xpm_filename[MAX_FILENAME];
  char png_filename[MAX_FILENAME];
  char geo_filename[MAX_FILENAME];
  char kml_filename[MAX_FILENAME];   // filename for kml file that describes the png file in keyhole markup language
  char timestring[101];  // string representation of the time heard or the current time
  FILE *f;
  FILE *fk;  // file handle for kml file
  time_t expire_time;
#ifdef HAVE_CONVERT
  char command[MAX_FILENAME*2];
#endif  // HAVE_CONVERT
  char temp_base_dir[MAX_VALUE];

  get_user_base_dir("tmp", temp_base_dir, sizeof(temp_base_dir));


  // The pthread_detach() call means we don't care about the
  // return code and won't use pthread_join() later.  Makes
  // threading more efficient.
  (void)pthread_detach(pthread_self());

  xastir_snprintf(xpm_filename,
                  sizeof(xpm_filename),
                  "%s/snapshot.xpm",
                  temp_base_dir);

  xastir_snprintf(png_filename,
                  sizeof(png_filename),
                  "%s/snapshot.png",
                  temp_base_dir);

  // Same for the .geo filename
  xastir_snprintf(geo_filename,
                  sizeof(geo_filename),
                  "%s/snapshot.geo",
                  temp_base_dir);

  // Same for the .kml filename
  xastir_snprintf(kml_filename,
                  sizeof(kml_filename),
                  "%s/snapshot.kml",
                  temp_base_dir);


  // Create a .geo file to match the new png image
  // Likewise for a matching .kml file
  f = fopen(geo_filename,"w");    // Overwrite whatever file
  // is there.
  fk = fopen(kml_filename,"w");

  if (f == NULL || fk == NULL)
  {
    if (f==NULL)
    {
      fprintf(stderr,"Couldn't open %s\n",geo_filename);
    }
    if (fk==NULL)
    {
      fprintf(stderr,"Couldn't open %s\n",kml_filename);
    }
  }
  else
  {
    float lat1, long1, lat2, long2;


    long1 = f_NW_corner_longitude;
    lat1 = f_NW_corner_latitude;
    long2 = f_SE_corner_longitude;
    lat2 = f_SE_corner_latitude;

    // FILENAME   world1.xpm
    // #          x          y        lon         lat
    // TIEPOINT   0          0        -180        90
    // TIEPOINT   639        319      180         -90
    // IMAGESIZE  640        320
    // REFRESH    250

    fprintf(f,"FILENAME     snapshot.png\n");
    fprintf(f,"#            x       y        lon           lat\n");
    fprintf(f,"TIEPOINT     0       0       %8.5f     %8.5f\n",
            long1, lat1);
    fprintf(f,"TIEPOINT     %-4d    %-4d    %8.5f     %8.5f\n",
            (int)screen_width-1, (int)screen_height-1, long2, lat2);

    fprintf(f,"IMAGESIZE    %-4d    %-4d\n",
            (int)screen_width, (int)screen_height);
    fprintf(f,"REFRESH      250\n");
    fclose(f);

    // Write a matching kml file that describes the location of the snapshot on
    // the Earth's surface.
    // Another kml file pointing to the location of this file with a networklinkcontrol element
    // and an update element loaded into a kml application should be able to reload this file
    // at regular intervals.
    // See kml documentation of:
    // <kml><NetworkLinkControl><linkName/><refreshMode/>
    //
    // <?xml version="1.0" encoding="UTF-8"?>
    // <kml xmlns="http://earth.google.com/kml/2.1">
    // <Document>
    //   <NetworkLink>
    //      <Link>
    //        <href>http://www.example.com/cgi-bin/screenshot.kml</href>
    //        <refreshMode>onExpire</refreshMode>
    //      </Link>
    //  </NetworkLink>
    // </Document>
    // </kml>
    //
    // TODO: Calculate a suitable range and tilt for viewing the snapshot draped on the
    // underlying terrain.

    fprintf(fk,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fk,"<kml xmlns=\"http://earth.google.com/kml/2.2\">\n");
    // Add an expire time matching the time when the next snapshot should
    // be produced, so that a network link with an onExpire refresh mode
    // will check for the next snapshot.
    expire_time = sec_now() + (time_t)(snapshot_interval * 60);
    if (get_w3cdtf_datetime(expire_time, timestring, False, False))
    {
      if (strlen(timestring) > 0)
      {
        fprintf(fk,"  <NetworkLinkControl>\n");
        fprintf(fk,"     <expires>%s</expires>\n",timestring);
        fprintf(fk,"  </NetworkLinkControl>\n");
      }
    }
    fprintf(fk,"  <Document>\n");
    fprintf(fk,"    <name>XASTIR Snapshot from %s</name>\n",my_callsign);
    fprintf(fk,"    <open>1</open>\n");
    fprintf(fk,"    <GroundOverlay>\n");
    fprintf(fk,"      <name>Xastir snapshot</name>\n");
    fprintf(fk,"      <visibility>1</visibility>\n");
    // timestamp the overlay with the current time
    if (get_w3cdtf_datetime(sec_now(), timestring, True, True))
    {
      if (strlen(timestring) > 0)
      {
        fprintf(fk,"      <TimeStamp><when>%s</when></TimeStamp>\n",timestring);
        fprintf(fk,"      <description>Overlay shows screen visible for %s in Xastir at %s.</description>\n",my_callsign,timestring);
      }
    }
    else
    {
      fprintf(fk,"      <description>Overlay shows screen visible for %s in Xastir.</description>\n",my_callsign);
    }
    fprintf(fk,"      <LookAt>\n");
    fprintf(fk,"        <longitude>%8.5f</longitude>\n",f_center_longitude);
    fprintf(fk,"        <latitude>%8.5f</latitude>\n",f_center_latitude);
    fprintf(fk,"        <altitude>0</altitude>\n");
    fprintf(fk,"        <range>30350.36838438907</range>\n");  // range in meters from viewer to lookat point
    fprintf(fk,"        <tilt>0</tilt>\n");  // 0 is looking straight down
    fprintf(fk,"        <altitudeMode>clampToGround</altitudeMode>\n");
    fprintf(fk,"        <heading>0</heading>\n");  // 0 is north at top, 90 east at top
    fprintf(fk,"      </LookAt>\n");
    fprintf(fk,"      <Icon>\n");
    fprintf(fk,"        <href>snapshot.png</href>\n");
    fprintf(fk,"      </Icon>\n");
    fprintf(fk,"      <LatLonBox>\n");
    fprintf(fk,"        <north>%8.5f</north>\n",lat1);
    fprintf(fk,"        <south>%8.5f</south>\n",lat2);
    fprintf(fk,"        <east>%8.5f</east>\n",long2);
    fprintf(fk,"        <west>%8.5f</west>\n",long1);
    fprintf(fk,"        <rotation>0</rotation>\n");
    fprintf(fk,"      </LatLonBox>\n");
    fprintf(fk,"    </GroundOverlay>\n");
    fprintf(fk,"  </Document>\n");
    fprintf(fk,"</kml>\n");

    fclose(fk);

    chmod( geo_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
    chmod( kml_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
  }


  if ( debug_level & 512 )
  {
    fprintf(stderr,"Convert %s ==> %s\n", xpm_filename, png_filename );
  }


#ifdef HAVE_CONVERT
  // Convert it to a png file.  This depends upon having the
  // ImageMagick command "convert" installed.
  strcpy(command, CONVERT_PATH);
  command[sizeof(command)-1] = '\0';  // Terminate string
  strcat(command, " -quality 100 -colors 256 ");
  command[sizeof(command)-1] = '\0';  // Terminate string
  strcat(command, xpm_filename);
  command[sizeof(command)-1] = '\0';  // Terminate string
  strcat(command, " ");
  command[sizeof(command)-1] = '\0';  // Terminate string
  strcat(command, png_filename);
  command[sizeof(command)-1] = '\0';  // Terminate string

  if ( system( command ) != 0 )
  {
    // We _may_ have had an error.  Check errno to make
    // sure.
    if (errno)
    {
      fprintf(stderr, "%s\n", strerror(errno));
      fprintf(stderr,
              "Failed to convert snapshot: %s -> %s\n",
              xpm_filename,
              png_filename);
    }
    else
    {
      fprintf(stderr,
              "System call return error: convert: %s -> %s\n",
              xpm_filename,
              png_filename);
    }
  }
  else
  {
    chmod( png_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

//        // Delete temporary xpm file
//        unlink( xpm_filename );

    if ( debug_level & 512 )
    {
      fprintf(stderr,"  Done creating png.\n");
    }
  }

#endif  // HAVE_CONVERT

  // Signify that we're all done and that another snapshot can
  // occur.
  doing_snapshot = 0;

  return(NULL);
}
#endif  // NO_XPM





// Starts a separate thread that creates a png image from the
// current displayed image.
//
void Snapshot(void)
{
#ifndef NO_XPM
  pthread_t snapshot_thread_id;
  char xpm_filename[MAX_FILENAME];
  int xpmretval;
#endif  // NO_XPM
  char temp_base_dir[MAX_VALUE];

  get_user_base_dir("tmp", temp_base_dir, sizeof(temp_base_dir));


  // Check whether we're already doing a snapshot
  if (doing_snapshot)
  {
    return;
  }

  // Time to take another snapshot?
  // New snapshot interval based on slider in Configure Timing
  // dialog (in minutes)
  if (sec_now() < (last_snapshot + (snapshot_interval * 60)) )
  {
    return;
  }

  last_snapshot = sec_now(); // Set up timer for next time


#ifndef NO_XPM

  if (debug_level & 512)
  {
    fprintf(stderr,"Taking Snapshot\n");
  }

  doing_snapshot++;

  // Set up the XPM filename that we'll use
  xastir_snprintf(xpm_filename,
                  sizeof(xpm_filename),
                  "%s/snapshot.xpm",
                  temp_base_dir);


  if ( debug_level & 512 )
  {
    fprintf(stderr,"Creating %s\n", xpm_filename );
  }

  // Create an XPM file from pixmap_final.
  if (chdir(temp_base_dir) != 0)
  {
    fprintf(stderr,"Couldn't chdir to %s directory for snapshot\n", temp_base_dir);
    return;
  }

  xpmretval=XpmWriteFileFromPixmap(XtDisplay(appshell),   // Display *display
                                   "snapshot.xpm",                             // char *filename
                                   pixmap_final,                               // Pixmap pixmap
                                   (Pixmap)NULL,                               // Pixmap shapemask
                                   NULL );

  if (xpmretval != XpmSuccess)
  {
    fprintf(stderr,"ERROR writing %s: %s\n", xpm_filename,
            XpmGetErrorString(xpmretval));
    return;
  }

  chmod( xpm_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );


//----- Start New Thread -----

  //
  // Here we start a new thread.  We'll communicate with the main
  // thread via global variables.  Use mutex locks if there might
  // be a conflict as to when/how we're updating those variables.
  //

  if (pthread_create(&snapshot_thread_id, NULL, snapshot_thread, NULL))
  {
    fprintf(stderr,"Error creating snapshot thread\n");
  }
  else
  {
    // We're off and running with the new thread!
  }
#endif  // NO_XPM
}





// Function to remove double-quote characters and spaces that occur
// outside of the double-quote characters.
void clean_string(char *input)
{
  char *i;
  char *j;


  //fprintf(stderr,"|%s|\t",input);

  // Remove any double quote characters
  i = index(input,'"');   // Find first quote character, if any

  if (i != NULL)
  {
    j = index(i+1,'"'); // Find second quote character, if any

    if (j != NULL)      // Found two quote characters
    {
      j[0] = '\0';    // Terminate the string at the 2nd quote
      // Can't use strcpy here because it can't work with
      // overlapping strings.  strcpy is a dangerous function
      // anyway and shouldn't be used.
      memmove(input, i+1, j-i);
    }
    else    // We only found one quote character.  What to do?
    {
//            fprintf(stderr,"clean_string: Only one quote found!\n");
    }
  }
  //fprintf(stderr,"|%s|\n",input);

  // Remove leading/trailing spaces?
}







// Test map visibility (on screen)
//
// Input parameters are in Xastir coordinate system (fastest for us)
// check_percentage:
//      0 = don't check
//      1 = check map size versus viewport scale.  Return 0 if map
//      is too large/small (percentage-wise) to be displayed.
//
// Returns: MAP_NOT_VIS if map is _not_ visible
//          MAP_IS_VIS if map _is_ visible
//
// Xastir Coordinate System:
//
//              0 (90 deg. or 90N)
//
// 0 (-180 deg. or 180W)      129,600,000 (180 deg. or 180E)
//
//          64,800,000 (-90 deg. or 90S)
//
// Note that we already have map_visible() and map_visible_lat_lon()
// routines.
//
enum map_onscreen_enum map_onscreen(long left,
                                    long right,
                                    long top,
                                    long bottom,
                                    int UNUSED(check_percentage) )
{

  enum map_onscreen_enum in_window = MAP_NOT_VIS;


  if (map_visible((unsigned long)bottom,
                  (unsigned long)top,
                  (unsigned long)left,
                  (unsigned long)right))
  {
    in_window = MAP_IS_VIS;
    //fprintf(stderr,"map_onscreen:Map is visible\n");
  }


#ifdef MAP_SCALE_CHECK
  // Check whether map is too large/small for our current scale?
  // Check whether the map extents are < XX% of viewscreen size
  // (both directions), or viewscreen is > XX% of map extents
  // (either direction).  This will knock out maps that are too
  // large/small to be displayed at this zoom level.
//WE7U
  if (in_window && check_percentage)
  {
    long map_x, map_y, view_x, view_y;
    float percentage = 0.04;


    map_x  = right - left;
    if (map_x < 0)
    {
      map_x = 0;
    }

    map_y  = bottom - top;
    if (map_y < 0)
    {
      map_y = 0;
    }

    view_x = max_NW_corner_longitude - NW_corner_longitude;
    if (view_x < 0)
    {
      view_x = 0;
    }

    view_y = max_NW_corner_latitude - NW_corner_latitude;
    if (view_y < 0)
    {
      view_y = 0;
    }

//fprintf(stderr,"\n map_x: %d\n", map_x);
//fprintf(stderr," map_y: %d\n", map_y);
//fprintf(stderr,"view_x: %d\n", view_x);
//fprintf(stderr,"view_y: %d\n", view_y);

    if ((map_x < (view_x * percentage )) && (map_y < (view_x * percentage)))
    {
      in_window = 0;  // Send back "not-visible" flag
      fprintf(stderr,"map too small for view: %d%%\n",(int)(percentage * 100));
    }

//        if ((view_x < (map_x * percentage)) && (view_y < (map_x * percentage))) {
//            in_window = 0;  // Send back "not-visible" flag
//fprintf(stderr,"view too small for map: %d%%\n",(int)(percentage * 100));
//        }

  }
#endif  // MAP_SCALE_CHECK

  //fprintf(stderr,"map_onscreen returning %d\n", in_window);
  return (in_window);
}





// Function which checks whether a map is onscreen, but does so by
// finding the map boundaries from the map index.  The only input
// parameter is the complete path/filename.
//
// Returns: MAP_NOT_VIS if map is _not_ visible
//          MAP_IS_VIS if map _is_ visible
//          MAP_NOT_INDEXED if the map is not in the index
//
enum map_onscreen_enum map_onscreen_index(char *filename)
{
  unsigned long top, bottom, left, right;
  enum map_onscreen_enum onscreen = MAP_NOT_INDEXED;
  int max_zoom, min_zoom;
  int map_layer, draw_filled, usgs_drg, auto_maps; // Unused in this function


  if (index_retrieve(filename, &bottom, &top, &left, &right,
                     &max_zoom, &min_zoom, &map_layer,
                     &draw_filled, &usgs_drg, &auto_maps) )
  {

    //fprintf(stderr, "Map found in index: %s\n", filename);

    // Map was in the index, check for visibility and scale
    // Check whether the map extents are < XX% of viewscreen
    // size (both directions), or viewscreen is > XX% of map
    // extents (either direction).  This will knock out maps
    // that are too large/small to be displayed at this zoom
    // level.
    if (map_onscreen(left, right, top, bottom, 1))
    {

      //fprintf(stderr, "Map found in index and onscreen: %s\n", filename);

      if (((max_zoom == 0) ||
           ((max_zoom != 0) && (scale_y <= max_zoom))) &&
          ((min_zoom == 0) ||
           ((min_zoom != 0) && (scale_y >= min_zoom))))
      {

        onscreen = MAP_IS_VIS;
        //fprintf(stderr,"Map in the zoom zone: %s\n",filename);
      }
      else
      {
        onscreen = MAP_NOT_VIS;
        //fprintf(stderr,"Map not in the zoom zone: %s\n",filename);
      }

// Check whether the map extents are < XX% of viewscreen size (both
// directions), or viewscreen is > XX% of map extents (either
// direction).  This will knock out maps that are too large/small to
// be displayed at this zoom level.


    }
    else    // Map is not visible
    {
      onscreen = MAP_NOT_VIS;
      //fprintf(stderr,"Map found in index but not onscreen: %s\n",filename);
    }
  }
  else    // Map is not in the index
  {
    onscreen = MAP_NOT_INDEXED;
    //fprintf(stderr,"Map not found in index: %s\n",filename);
  }
  return(onscreen);
}





/**********************************************************
 * draw_map()
 *
 * Function which tries to figure out what type of map or
 * image file we're dealing with, and takes care of getting
 * it onto the screen.  Calls other functions to deal with
 * .geo/.tif/.shp maps.
 *
 * If destination_pixmap == DRAW_NOT, then we'll not draw
 * the map anywhere, but we'll determine the map extents
 * and write them to the map index file.
 **********************************************************/
/* table of map drivers, selected by filename extension */
extern void draw_dos_map(Widget w,
                         char *dir,
                         char *filenm,
                         alert_entry *alert,
                         u_char alert_color,
                         int destination_pixmap,
                         map_draw_flags *draw_flags);

extern void draw_palm_image_map(Widget w,
                                char *dir,
                                char *filenm,
                                alert_entry *alert,
                                u_char alert_color,
                                int destination_pixmap,
                                map_draw_flags *draw_flags);

#ifdef HAVE_LIBSHP
extern void draw_shapefile_map (Widget w,
                                char *dir,
                                char *filenm,
                                alert_entry *alert,
                                u_char alert_color,
                                int destination_pixmap,
                                map_draw_flags *draw_flags);
#ifdef WITH_DBFAWK
  extern void clear_dbfawk_sigs(void);
#endif /* WITH_DBFAWK */
#endif /* HAVE_LIBSHP */
#ifdef HAVE_LIBGEOTIFF
extern void draw_geotiff_image_map(Widget w,
                                   char *dir,
                                   char *filenm,
                                   alert_entry *alert,
                                   u_char alert_color,
                                   int destination_pixmap,
                                   map_draw_flags *draw_flags);
#endif /* HAVE_LIBGEOTIFF */
extern void draw_geo_image_map(Widget w,
                               char *dir,
                               char *filenm,
                               alert_entry *alert,
                               u_char alert_color,
                               int destination_pixmap,
                               map_draw_flags *draw_flags);

extern void draw_gnis_map(Widget w,
                          char *dir,
                          char *filenm,
                          alert_entry *alert,
                          u_char alert_color,
                          int destination_pixmap,
                          map_draw_flags *draw_flags);

extern void draw_pop_map(Widget w,
                         char *dir,
                         char *filenm,
                         alert_entry *alert,
                         u_char alert_color,
                         int destination_pixmap,
                         map_draw_flags *draw_flags);

struct
{
  char *ext;
  enum {none=0, map, tif, geo, gnis, shp, pop} type;
  void (*func)(Widget w,
               char *dir,
               char *filenm,
               alert_entry *alert,
               u_char alert_color,
               int destination_pixmap,
               map_draw_flags *draw_flags);
} map_driver[] =
{
  {"map",map,draw_dos_map},

#ifdef HAVE_LIBGEOTIFF
  {"tif",tif,draw_geotiff_image_map},
#endif /* HAVE_LIBGEOTIFF */

  {"geo",geo,draw_geo_image_map},
  {"gnis",gnis,draw_gnis_map},
  {"pop",pop,draw_pop_map},

#ifdef HAVE_LIBSHP
  {"shp",shp,draw_shapefile_map},
#endif /* HAVE_LIBSHP */

  {NULL,none,NULL}
}, *map_driver_ptr;





void draw_map (Widget w, char *dir, char *filenm, alert_entry *alert,
               u_char alert_color, int destination_pixmap,
               map_draw_flags *draw_flags)
{
  enum map_onscreen_enum onscreen;
  char *ext;
  char file[MAX_FILENAME];

  if ((ext = get_map_ext(filenm)) == NULL)
  {
    return;
  }

  if (debug_level & 16)
  {
    fprintf(stderr,"draw_map: Searching for map driver\n");
  }

  for (map_driver_ptr = map_driver; map_driver_ptr->ext; map_driver_ptr++)
  {
    if (strcasecmp(ext,map_driver_ptr->ext) == 0)
    {
      if (debug_level & 16)
        fprintf(stderr,
                "draw_map: Found map driver: %s: %d\n",
                ext,
                map_driver_ptr->type);
      break;            /* found our map_driver */
    }
  }
  if (map_driver_ptr->type == none)      /* fall thru: unknown map driver */
  {
    // Check whether we're indexing or drawing the map
    if ( (destination_pixmap != INDEX_CHECK_TIMESTAMPS)
         && (destination_pixmap != INDEX_NO_TIMESTAMPS) )
    {
      // We're drawing, not indexing.  Output a warning
      // message.
      fprintf(stderr,"*** draw_map: Unknown map type: %s ***\n", filenm);
    }
    else    // We're indexing
    {
      if (debug_level & 16)
      {
        fprintf(stderr,"draw_map: No map driver found\n");
      }
    }
    return;
  }

  onscreen = map_onscreen_index(filenm); // Check map index

  // Check whether we're indexing or drawing the map
  if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
       || (destination_pixmap == INDEX_NO_TIMESTAMPS) )
  {

    // We're indexing maps
    if (onscreen != MAP_NOT_INDEXED) // We already have an index entry for this map.
      // This is where we pick up a big speed increase:
      // Refusing to index a map that's already indexed.
    {
      return;  // Skip it.
    }
  }
  else    // We're drawing maps
  {
    // See if map is visible.  If not, skip it.
    if (onscreen == MAP_NOT_VIS)    // Map is not visible, skip it.
    {
      //fprintf(stderr,"map not visible\n");
      if (alert)
      {
        alert->flags[on_screen] = 'N';
      }
      return;
    }
  }


  xastir_snprintf(file, sizeof(file), "%s/%s", dir, filenm);

  // Used for debugging.  If we get a segfault on a map, this is
  // often the only way of finding out which map file we can't
  // handle.
  if (debug_level & 16)
  {
    fprintf(stderr,"draw_map: %s\n",file);
  }

  /* XXX - aren't alerts just shp maps?  Why was there special case code? */

  if (map_driver_ptr->func)
  {
    map_driver_ptr->func(w,
                         dir,
                         filenm,
                         alert,
                         alert_color,
                         destination_pixmap,
                         draw_flags);
  }

  XmUpdateDisplay (XtParent (da));
}  // End of draw_map()





static void index_update_directory(char *directory);
static void index_update_accessed(char *filename);





/////////////////////////////////////////////////////////////////////
// map_search()
//
// Function which recurses through map directories, finding map
// files.  It's called from load_auto_maps and load_alert_maps.  If
// a map file is found, it is drawn.  We can also call this function
// in indexing mode rather than draw mode, specified by the
// destination_pixmap parameter.
//
// If alert == NULL, we looking for a regular map file to draw.
// If alert != NULL, we have a weather alert to draw.
//
// For alert maps, we need to do things a bit differently, as there
// should be only a few maps that contain all of the alert maps, and we
// can compute which map some of them might be in.  We need to fill in
// the alert structure with the filename that alert is found in.
// For alerts we're not drawing the maps, we're just computing the
// full filename for the alert and filling that struct field in.
//
// The "warn" parameter specifies whether to warn the operator about
// the alert on the console as well.  If it was received locally or
// via local RF, then the answer is yes.  The severe weather may be
// nearby.
//
// We have the timestamp of the map_index.sys file stored away in
// the global:  time_t map_index_timestamp;
// Use that timestamp to compare the map file or GEO file timestamps
// to.  Re-index the map if map_index_timestamp is older.
//
/////////////////////////////////////////////////////////////////////
static void map_search (Widget w, char *dir, alert_entry * alert, int *alert_count,int warn, int destination_pixmap)
{
  struct dirent *dl = NULL;
  DIR *dm;
  char fullpath[MAX_FILENAME];
  struct stat nfile;
//    const time_t *ftime;
//    char this_time[40];
  char *ptr;
  char *map_dir;
  int map_dir_length;
  map_draw_flags mdf;

  // We'll use the weather alert directory if it's an alert
  map_dir = alert ? ALERT_MAP_DIR : SELECTED_MAP_DIR;

  map_dir_length = (int)strlen (map_dir);

  if (alert)      // We're doing weather alerts
  {
    // First check whether the alert->filename variable is filled
    // in.  If so, we've already found the file and can just display
    // that shape in the file
    if (alert->filename[0] == '\0')     // No filename in struct, so will have
    {
      // to search for the shape in the files.
      switch (alert->title[3])
      {
        case 'F':   // 'F' in 4th char means fire alert
          // Use fire alert file fz_??????
          //fprintf(stderr,"%c:Fire Alert file\n",alert->title[3]);
          xastir_snprintf(alert->filename,
                          sizeof(alert->filename),
                          "fz");
          break;

        case 'C':   // 'C' in 4th char means county
          // Use County file c_??????
          //fprintf(stderr,"%c:County file\n",alert->title[3]);
          xastir_snprintf(alert->filename,
                          sizeof(alert->filename),
                          "c_");
          break;
        case 'A':   // 'A' in 4th char means county warning area
          // Use County warning area w_?????
          //fprintf(stderr,"%c:County warning area file\n",alert->title[3]);
          xastir_snprintf(alert->filename,
                          sizeof(alert->filename),
                          "w_");
          break;
        case 'Z':
          // Zone, coastal or offshore marine zone file z_????? or mz?????? or oz??????
          // oz: ANZ081-086,088,PZZ081-085
          // mz: AM,AN,GM,LC,LE,LH,LM,LO,LS,PH,PK,PM,PS,PZ,SL
          // z_: All others
          if (strncasecmp(alert->title,"AM",2) == 0)
          {
            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "mz");
          }
          else if (strncasecmp(alert->title,"AN",2) == 0)
          {
            // Need to check for Z081-Z086, Z088, if so use
            // oz??????, else use mz??????
            if (       (strncasecmp(&alert->title[3],"Z081",4) == 0)
                       || (strncasecmp(&alert->title[3],"Z082",4) == 0)
                       || (strncasecmp(&alert->title[3],"Z083",4) == 0)
                       || (strncasecmp(&alert->title[3],"Z084",4) == 0)
                       || (strncasecmp(&alert->title[3],"Z085",4) == 0)
                       || (strncasecmp(&alert->title[3],"Z086",4) == 0)
                       || (strncasecmp(&alert->title[3],"Z088",4) == 0) )
            {
              //fprintf(stderr,"%c:Offshore marine zone file\n",alert->title[3]);
              xastir_snprintf(alert->filename,
                              sizeof(alert->filename),
                              "oz");
            }
            else
            {
              //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
              xastir_snprintf(alert->filename,
                              sizeof(alert->filename),
                              "mz");
            }
          }
          else if (strncasecmp(alert->title,"GM",2) == 0)
          {
            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "mz");
          }
          else if (strncasecmp(alert->title,"LC",2) == 0)
          {
            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "mz");
          }
          else if (strncasecmp(alert->title,"LE",2) == 0)
          {
            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "mz");
          }
          else if (strncasecmp(alert->title,"LH",2) == 0)
          {
            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "mz");
          }
          else if (strncasecmp(alert->title,"LM",2) == 0)
          {
            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "mz");
          }
          else if (strncasecmp(alert->title,"LO",2) == 0)
          {
            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "mz");
          }
          else if (strncasecmp(alert->title,"LS",2) == 0)
          {
            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "mz");
          }
          else if (strncasecmp(alert->title,"PH",2) == 0)
          {
            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "mz");
          }
          else if (strncasecmp(alert->title,"PK",2) == 0)
          {
            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "mz");
          }
          else if (strncasecmp(alert->title,"PM",2) == 0)
          {
            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "mz");
          }
          else if (strncasecmp(alert->title,"PS",2) == 0)
          {
            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "mz");
          }
          else if (strncasecmp(alert->title,"PZ",2) == 0)
          {
// Need to check for PZZ081-085, if so use oz??????, else use mz??????
            if (       (strncasecmp(&alert->title[3],"Z081",4) == 0)
                       || (strncasecmp(&alert->title[3],"Z082",4) == 0)
                       || (strncasecmp(&alert->title[3],"Z083",4) == 0)
                       || (strncasecmp(&alert->title[3],"Z084",4) == 0)
                       || (strncasecmp(&alert->title[3],"Z085",4) == 0) )
            {
              //fprintf(stderr,"%c:Offshore marine zone file\n",alert->title[3]);
              xastir_snprintf(alert->filename,
                              sizeof(alert->filename),
                              "oz");
            }
            else
            {
              //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
              xastir_snprintf(alert->filename,
                              sizeof(alert->filename),
                              "mz");
            }
          }
          else if (strncasecmp(alert->title,"SL",2) == 0)
          {
            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "mz");
          }
          else
          {
            // Must be regular zone file instead of coastal
            // marine zone or offshore marine zone.
            //fprintf(stderr,"%c:Zone file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "z_");
          }
          break;

        default:
// VK2XJG
// This section could most likely be moved so that it's not called as part of the default, but in order
// to get the shapefiles for BOM working this was the best spot at the time...
          // Australian BOM alerts use the following shapefiles:
          // PW = Public Warning = gfe_public_weather
          // MW = Coastal Waters = gfe_coastal_waters
          // CW = Coastal Waters Warnings = gfe_coastal_waters_warnings
          // FW = Fire Weather = gfe_fire_weather
          // ME = Metro Effects = gfe_metro_areas
          // Note - Need to cater for both 2 and 3 character state designators
          // Shapefile filenames are static - there is no datestamp in the filename.
          if ((strncasecmp(&alert->title[4],"MW",2) == 0) || (strncasecmp(&alert->title[3],"MW",2) == 0))
          {
            //fprintf(stderr,"%c:BOM Coastal Waters file\n",alert->title[4]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "gfe_coastal_waters.shp");
          }
          else if ((strncasecmp(&alert->title[4],"CW",2) == 0) || (strncasecmp(&alert->title[3],"CW",2) == 0))
          {
            //fprintf(stderr,"%c:BOM Coastal waters warning file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "gfe_coastal_waters_warnings.shp");
          }
          else if ((strncasecmp(&alert->title[4],"PW",2) == 0) || (strncasecmp(&alert->title[3],"PW",2) == 0))
          {
            //fprintf(stderr,"%c:BOM Public Weather file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "gfe_public_weather.shp");
          }
          else if ((strncasecmp(&alert->title[4],"FW",2) == 0) || (strncasecmp(&alert->title[3],"FW",2) == 0))
          {
            //fprintf(stderr,"%c:BOM Fire Weather file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "gfe_fire_weather.shp");
          }
          else if ((strncasecmp(&alert->title[4],"ME",2) == 0) || (strncasecmp(&alert->title[3],"ME",2) == 0))
          {
            //fprintf(stderr,"%c:BOM Metro Areas file\n",alert->title[3]);
            xastir_snprintf(alert->filename,
                            sizeof(alert->filename),
                            "gfe_metro_areas.shp");
          }



          // Unknown type
//fprintf(stderr,"%c:Can't match weather warning to a Shapefile:%s\n",alert->title[3],alert->title);
          break;
      }
//            fprintf(stderr,"%s\t%s\t%s\n",alert->activity,alert->alert_status,alert->title);
      //fprintf(stderr,"File: %s\n",alert->filename);
    }

// NOTE:  Need to skip this part if we have a full filename.

    if (alert->filename[0])     // We have at least a partial filename
    {
      int done = 0;

      if (strlen(alert->filename) > 3)
      {
        done++;  // We already have a filename
      }

      if (!done)      // We don't have a filename yet
      {

        // Look through the warning directory to find a match for
        // the first few characters that we already figured out.
        // This is designed so that we don't need to know the exact
        // filename, but only the lead three characters in order to
        // figure out which shapefile to use.
        dm = opendir (dir);
        if (!dm)    // Couldn't open directory
        {
          xastir_snprintf(fullpath, sizeof(fullpath), "aprsmap %s", dir);
          // If local alert, warn the operator via the
          // console as well.
          if (warn)
          {
            perror (fullpath);
          }
        }
        else      // We could open the directory just fine
        {
          while ( (dl = readdir(dm)) && !done )
          {
            int i;

            // Check the file/directory name for control
            // characters
            for (i = 0; i < (int)strlen(dl->d_name); i++)
            {
              // Dump out a warning if control
              // characters other than LF or CR are
              // found.
              if ( (dl->d_name[i] != '\n')
                   && (dl->d_name[i] != '\r')
                   && (dl->d_name[i] < 0x20) )
              {

                fprintf(stderr,"\nmap_search: Found control char 0x%02x in alert file/alert directory name.  Line was:\n",
                        dl->d_name[i]);
                fprintf(stderr,"%s\n",dl->d_name);
              }
              /*
              // This part might not work 'cuz we'd be changing a memory area that
              // we might have only read access to.  Check this.
                                          if (dl->d_name[i] < 0x20) {
                                              // Terminate string at any control character
                                              dl->d_name[i] = '\0';
                                          }
              */
            }

            xastir_snprintf(fullpath, sizeof(fullpath), "%s%s", dir, dl->d_name);
            /*fprintf(stderr,"FULL PATH %s\n",fullpath); */
            if (stat (fullpath, &nfile) == 0)
            {
//                            ftime = (time_t *)&nfile.st_ctime;
              switch (nfile.st_mode & S_IFMT)
              {
                case (S_IFDIR):     // It's a directory, skip it
                  break;

                case (S_IFREG):     // It's a file, check it
                  /*fprintf(stderr,"FILE %s\n",dl->d_name); */
                  // Here we look for a match for the
                  // first 2 characters of the filename.
                  //
                  if (strncasecmp(alert->filename,dl->d_name,2) == 0)
                  {
                    // We have a match for the
                    // first few characters.
                    // Check that last three are
                    // "shp"

                    //fprintf(stderr,"%s\n",fullpath);

                    if ( (dl->d_name[strlen(dl->d_name)-3] == 's'
                          || dl->d_name[strlen(dl->d_name)-3] == 'S')
                         && (dl->d_name[strlen(dl->d_name)-2] == 'h'
                             || dl->d_name[strlen(dl->d_name)-2] == 'H')
                         && (dl->d_name[strlen(dl->d_name)-1] == 'p'
                             || dl->d_name[strlen(dl->d_name)-1] == 'P') )
                    {
                      // We have an exact match.
                      // Save the filename in the alert
                      memcpy(alert->filename,
                             dl->d_name,
                             sizeof(alert->filename));
                      // Terminate string
                      alert->filename[sizeof(alert->filename)-1] = '\0';
                      done++;
                      //fprintf(stderr,"%s\n",dl->d_name);
                    }
                  }
                  break;

                default:    // Not dir or file, skip it
                  break;
              }
            }
          }
        }
        (void)closedir (dm);
      }

      if (done)      // We found a filename match for the alert
      {
        // Go draw the weather alert (kind'a)
//WE7U
        mdf.draw_filled=1;
        mdf.usgs_drg=0;

        if (debug_level & 16)
        {
          fprintf(stderr,"map_search: calling draw_map for an alert\n");
        }

        draw_map (w,
                  dir,                // Alert directory
                  alert->filename,    // Shapefile filename
                  alert,
                  -1,                 // Signifies "DON'T DRAW THE SHAPE"
                  destination_pixmap,
                  &mdf );

        if (debug_level & 16)
        {
          fprintf(stderr,"map_search: returned from draw_map\n");
        }

      }
      else        // No filename found that matches the first two
      {
        // characters that we already computed.

        //
        // Need code here
        //

      }
    }
    else    // Still no filename for the weather alert.
    {
      // Output an error message?
      //
      // Need code here
      //

    }
  }


// MAPS, not alerts

  else    // We're doing regular maps, not weather alerts
  {
    time_t map_timestamp;


    dm = opendir (dir);
    if (!dm)    // Couldn't open directory
    {
      xastir_snprintf(fullpath, sizeof(fullpath), "aprsmap %s", dir);
      if (warn)
      {
        perror (fullpath);
      }
    }
    else
    {
      int count = 0;
      while ((dl = readdir (dm)))
      {
        int i;

        // Check the file/directory name for control
        // characters
        for (i = 0; i < (int)strlen(dl->d_name); i++)
        {
          // Dump out a warning if control characters
          // other than LF or CR are found.
          if ( (dl->d_name[i] != '\n')
               && (dl->d_name[i] != '\r')
               && (dl->d_name[i] < 0x20) )
          {

            fprintf(stderr,"\nmap_search: Found control char 0x%02x in map file/map directory name.  Line was:\n",
                    dl->d_name[i]);
            fprintf(stderr,"%s\n",dl->d_name);
          }
          /*
          // This part might not work 'cuz we'd be changing a memory area that
          // we might have only read access to.  Check this.
                              if (dl->d_name[i] < 0x20) {
                                  // Terminate string at any control character
                                  dl->d_name[i] = '\0';
                              }
          */
        }

        xastir_snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, dl->d_name);
        //fprintf(stderr,"FULL PATH %s\n",fullpath);
        if (stat (fullpath, &nfile) == 0)
        {
//                    ftime = (time_t *)&nfile.st_ctime;
          switch (nfile.st_mode & S_IFMT)
          {
            case (S_IFDIR):     // It's a directory, recurse
              //fprintf(stderr,"file %c letter %c\n",dl->d_name[0],letter);
              if ((strcmp (dl->d_name, ".") != 0) && (strcmp (dl->d_name, "..") != 0))
              {

                //fprintf(stderr,"FULL PATH %s\n",fullpath);

                // If we're indexing, throw the
                // directory into the map index as
                // well.
                if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
                     || (destination_pixmap == INDEX_NO_TIMESTAMPS) )
                {
                  char temp_dir[MAX_FILENAME];

                  // Drop off the base part of the
                  // path for the indexing,
                  // usually
                  // "/usr/local/share/xastir/maps".
                  // Add a '/' to the end.
                  xastir_snprintf(temp_dir,
                                  sizeof(temp_dir),
                                  "%s/",
                                  &fullpath[map_dir_length+1]);

                  // Add the directory to the
                  // in-memory map index.
                  index_update_directory(temp_dir);
                }

//                                xastir_snprintf(this_time,
//                                    sizeof(this_time),
//                                    "%s",
//                                    ctime(ftime));
                map_search(w, fullpath, alert, alert_count, warn, destination_pixmap);
              }
              break;

            case (S_IFREG):     // It's a file, draw the map
              /*fprintf(stderr,"FILE %s\n",dl->d_name); */

              // Get the last-modified timestamp for the map file
              //map_timestamp = (time_t)nfile.st_mtime;
              map_timestamp =
                (time_t)( (nfile.st_mtime>nfile.st_ctime) ? nfile.st_mtime : nfile.st_ctime );


              // Check whether we're doing indexing or
              // map drawing.  If indexing, we only
              // want to index if the map timestamp is
              // newer than the index timestamp.
              if (destination_pixmap == INDEX_CHECK_TIMESTAMPS
                  || destination_pixmap == INDEX_NO_TIMESTAMPS)
              {
                // We're doing indexing, not map drawing
                char temp_dir[MAX_FILENAME];

                // Drop off the base part of the
                // path for the indexing, usually
                // "/usr/local/share/xastir/maps".
                xastir_snprintf(temp_dir,
                                sizeof(temp_dir),
                                "%s",
                                &fullpath[map_dir_length+1]);

                // Update the "accessed"
                // variable in the record
                index_update_accessed(temp_dir);

// Note:  This is not as efficient as it should be, as we're looking
// through the in-memory map index here just to update the
// "accessed" variable, then in some cases looking through it again
// in the next section for updated maps, or if we're ignoring
// timestamps while indexing.  Looking through a linear linked list
// too many times overall.

                if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
                     && (map_timestamp < map_index_timestamp) )
                {
                  // Map is older than index _and_
                  // we're supposed to check
                  // timestamps.
                  count++;
                  break;  // Skip indexing this file
                }
                else    // Map is newer or we're ignoring timestamps.
                {
                  // We'll index the map
                  if (debug_level & 16)
                  {
                    fprintf(stderr,"Indexing map: %s\n",fullpath);
                  }
                }
              }

              // Check whether the file is in a subdirectory
              if (strncmp (fullpath, map_dir, (size_t)map_dir_length) != 0)
              {

                if (debug_level & 16)
                {
                  fprintf(stderr,"Calling draw_map\n");
                }
                mdf.draw_filled=1;
                mdf.usgs_drg=0;

                draw_map (w,
                          dir,
                          dl->d_name,
                          alert ? &alert[*alert_count] : NULL,
                          '\0',
                          destination_pixmap,
                          &mdf );

                if (debug_level & 16)
                {
                  fprintf(stderr,"Returned from draw_map\n");
                }
                if (alert_count && *alert_count)
                {
                  (*alert_count)--;
                }
              }
              else
              {
                // File is in the main map directory
                // Find the '/' character
                for (ptr = &fullpath[map_dir_length]; *ptr == '/'; ptr++) ;
                mdf.draw_filled=1;
                mdf.usgs_drg=0;

                if (debug_level & 16)
                {
                  fprintf(stderr,"Calling draw_map\n");
                }

                draw_map (w,
                          map_dir,
                          ptr,
                          alert ? &alert[*alert_count] : NULL,
                          '\0',
                          destination_pixmap,
                          &mdf );

                if (alert_count && *alert_count)
                {
                  (*alert_count)--;
                }
              }
              count++;
              break;

            default:
              break;
          }
        }
      }
      if (debug_level & 16)
      {
        fprintf(stderr,"Number of maps queried: %d\n", count);
      }

      (void)closedir (dm);
    }
  }
}





// List pointer for the map index linked list.
map_index_record *map_index_head = NULL;

// Might wish to have another variable in the index which is used to
// record that a file has been indexed recently.  This could be used
// to prune old entries out of the index if a full indexing didn't
// touch a file entry.  Could also delete an entry from the index
// if/when a file can't be opened?





// Function to dissect and free all of the records in a map index
// linked list, leaving it totally empty.
//
static void free_map_index(map_index_record *index_list_head)
{
  map_index_record *current;
  map_index_record *temp;


  current = index_list_head;

  while (current != NULL)
  {
    temp = current;
    if (current->XmStringPtr != NULL)
    {
      XmStringFree(current->XmStringPtr);
    }
    current = current->next;
    free(temp);
  }

  index_list_head = NULL;
}





// Function to copy just the properties fields from the backup map
// index to the primary index.  Must match each record before
// copying.  Once it's done, it frees the backup map index.
//
static void map_index_copy_properties(map_index_record *primary_index_head,
                                      map_index_record *backup_index_head)
{
  map_index_record *primary;
  map_index_record *backup;


  backup = backup_index_head;

  // Walk the backup list, comparing the filename field with the
  // primary list.  When a match is found, copy just the
  // Properties fields (map_layer/draw_filled/auto_maps/selected)
  // across to the primary record.
  //
  while (backup != NULL)
  {
    int done = 0;

    primary = primary_index_head;

    while (!done && primary != NULL)
    {

      if (strcmp(primary->filename, backup->filename) == 0)   // If match
      {

        if (debug_level & 16)
        {
          fprintf(stderr,"Match: %s\t%s\n",
                  primary->filename,
                  backup->filename);
        }

        // Copy the Properties across
        primary->max_zoom    = backup->max_zoom;
        primary->min_zoom    = backup->min_zoom;
        primary->map_layer   = backup->map_layer;
        primary->draw_filled = backup->draw_filled;
        primary->usgs_drg    = backup->usgs_drg;
        primary->auto_maps   = backup->auto_maps;
        primary->selected    = backup->selected;

        // Done copying this backup record.  Go on to the
        // next.  Skip the rest of the primary list for this
        // iteration.
        done++;
      }
      else    // No match, walk the primary list looking for one.
      {
        primary = primary->next;
      }
    }

    // Walk the backup list
    backup = backup->next;
  }

  // We're done copying.  Free the backup list.
  free_map_index(backup_index_head);
}





// Function used to add map directories to the in-memory map index.
// Causes an update of the index list in memory.  Input Records are
// inserted in alphanumerical order.  We mark directories in the
// index with a '/' on the end of the name, and zero entries for
// top/bottom/left/right.
// The input directory to this routine MUST have a '/' character on
// the end of it.  This is how we differentiate directories from
// files in the list.
static void index_update_directory(char *directory)
{

  map_index_record *current = map_index_head;
  map_index_record *previous = map_index_head;
  map_index_record *temp_record = NULL;
  int done = 0;
  int i;


  //fprintf(stderr,"index_update_directory: %s\n", directory );

  // Check for initial bad input
  if ( (directory == NULL)
       || (directory[0] == '\0')
       || (directory[strlen(directory) - 1] != '/')
       || ( (directory[1] == '/') && (strlen(directory) == 1)) )
  {
    fprintf(stderr,"index_update_directory: Bad input: %s\n",directory);
    return;
  }
  // Make sure there aren't any weird characters in the directory
  // that might cause problems later.  Look for control characters
  // and convert them to string-end characters.
  for ( i = 0; i < (int)strlen(directory); i++ )
  {
    // Change any control characters to '\0' chars
    if (directory[i] < 0x20)
    {

      fprintf(stderr,"\nindex_update_directory: Found control char 0x%02x in map file/map directory name:\n%s\n",
              directory[i],
              directory);

      directory[i] = '\0';    // Terminate it here
    }
  }
  // Check if the string is _now_ bogus
  if ( (directory[0] == '\0')
       || (directory[strlen(directory) - 1] != '/')
       || ( (directory[1] == '/') && (strlen(directory) == 1)))
  {
    fprintf(stderr,"index_update_directory: Bad input: %s\n",directory);
    return;
  }

  //if (map_index_head == NULL)
  //    fprintf(stderr,"Empty list\n");

  // Search for a matching directory name in the linked list
  while ((current != NULL) && !done)
  {
    int test;

    //fprintf(stderr,"Comparing %s to\n          %s\n",
    //    current->filename, directory);

    test = strcmp(current->filename, directory);
    if (test == 0)
    {
      // Found a match!
      //fprintf(stderr,"Found: Updating entry for %s\n",directory);
      temp_record = current;
      done++; // Exit loop, "current" points to found record
    }
    else if (test > 0)      // Found a string past us in the
    {
      // alphabet.  Insert ahead of this
      // last record.

      //fprintf(stderr,"\n%s\n%s\n",current->filename,directory);

      //fprintf(stderr,"Not Found: Inserting an index record for %s\n",directory);
      temp_record = (map_index_record *)malloc(sizeof(map_index_record));
      CHECKMALLOC(temp_record);

      if (current == map_index_head)    // Start of list!
      {
        // Insert new record at head of list
        temp_record->next = map_index_head;
        map_index_head = temp_record;
        //fprintf(stderr,"Inserting at head of list\n");
      }
      else    // Insert between "previous" and "current"
      {
        // Insert new record before "current"
        previous->next = temp_record;
        temp_record->next = current;
        //fprintf(stderr,"Inserting before current\n");
      }

      //fprintf(stderr,"Adding:%d:%s\n",strlen(directory),directory);

      // Fill in some default values for the new record.
      temp_record->selected = 0;
      temp_record->auto_maps = 0;
      temp_record->XmStringPtr = NULL;

      //current = current->next;
      done++;
    }
    else    // Haven't gotten to the correct insertion point yet
    {
      previous = current; // Save ptr to last record
      current = current->next;
    }
  }

  if (!done)      // Matching record not found, add a record to
  {
    // the end of the list.  "previous" points to
    // the last record in the list or NULL (empty
    // list).
    //fprintf(stderr,"Not Found: Adding an index record for %s\n",directory);
    temp_record = (map_index_record *)malloc(sizeof(map_index_record));
    CHECKMALLOC(temp_record);

    temp_record->next = NULL;

    if (previous == NULL)   // Empty list
    {
      map_index_head = temp_record;
      //fprintf(stderr,"First record in new list\n");
    }
    else    // Else at end of list
    {
      previous->next = temp_record;
      //fprintf(stderr,"Adding to end of list: %s\n",directory);
    }

    //fprintf(stderr,"Adding:%d:%s\n",strlen(directory),directory);

    // Fill in some default values for the new record.
    temp_record->selected = 0;
    temp_record->auto_maps = 0;
    temp_record->XmStringPtr = NULL;
  }

  // Update the values.  By this point we have a struct to fill
  // in, whether it's a new or old struct doesn't matter.  Convert
  // the values from lat/long to Xastir coordinate system.
  xastir_snprintf(temp_record->filename,MAX_FILENAME,"%s",directory);

  temp_record->bottom = 0;
  temp_record->top = 0;
  temp_record->left = 0;
  temp_record->right = 0;
  temp_record->accessed = 1;
  temp_record->max_zoom = 0;
  temp_record->min_zoom = 0;
  temp_record->map_layer = 0;
  temp_record->draw_filled = 0;
  temp_record->usgs_drg = 2;
}





// Function called by the various draw_* functions when in indexing
// mode.  Causes an update of the index list in memory.  Input
// parameters are in the Xastir coordinate system due to speed
// considerations.  Records are inserted in alphanumerical order.
void index_update_xastir(char *filename,
                         unsigned long bottom,
                         unsigned long top,
                         unsigned long left,
                         unsigned long right,
                         int default_map_layer)
{

  map_index_record *current = map_index_head;
  map_index_record *previous = map_index_head;
  map_index_record *temp_record = NULL;
  int done = 0;
  int i;


  // Check for initial bad input
  if ( (filename == NULL)
       || (filename[0] == '\0')
       || (filename[strlen(filename) - 1] == '/') )
  {
    fprintf(stderr,"index_update_xastir: Bad input: %s\n",filename);
    return;
  }
  // Make sure there aren't any weird characters in the filename
  // that might cause problems later.  Look for control characters
  // and convert them to string-end characters.
  for ( i = 0; i < (int)strlen(filename); i++ )
  {
    // Change any control characters to '\0' chars
    if (filename[i] < 0x20)
    {

      fprintf(stderr,"\nindex_update_xastir: Found control char 0x%02x in map file/map directory name:\n%s\n",
              filename[i],
              filename);

      filename[i] = '\0';    // Terminate it here
    }
  }
  // Check if the string is _now_ bogus
  if (filename[0] == '\0')
  {
    fprintf(stderr,"index_update_xastir: Bad input: %s\n",filename);
    return;
  }

  //fprintf(stderr,"index_update_xastir: (%lu,%lu)\t(%lu,%lu)\t%s\n",
  //    bottom, top, left, right, filename );

  //if (map_index_head == NULL)
  //    fprintf(stderr,"Empty list\n");

  // Skip dbf and shx map extensions.  Really should make this
  // case-independent...
  if (       strstr(filename,"shx")
             || strstr(filename,"dbf")
             || strstr(filename,"SHX")
             || strstr(filename,"DBF") )
  {
    return;
  }

  // Search for a matching filename in the linked list
  while ((current != NULL) && !done)
  {
    int test;

    //fprintf(stderr,"Comparing %s to\n          %s\n",current->filename,filename);

    test = strcmp(current->filename,filename);
    if (test == 0)
    {
      // Found a match!
      //fprintf(stderr,"Found: Updating entry for %s\n",filename);
      temp_record = current;
      done++; // Exit the while loop
    }
    else if (test > 0)      // Found a string past us in the
    {
      // alphabet.  Insert ahead of this
      // last record.

      //fprintf(stderr,"\n%s\n%s\n",current->filename,filename);

      //fprintf(stderr,"Not Found: Inserting an index record for %s\n",filename);
      temp_record = (map_index_record *)malloc(sizeof(map_index_record));
      CHECKMALLOC(temp_record);

      if (current == map_index_head)    // Start of list!
      {
        // Insert new record at head of list
        temp_record->next = map_index_head;
        map_index_head = temp_record;
        //fprintf(stderr,"Inserting at head of list\n");
      }
      else
      {
        // Insert new record before "current"
        previous->next = temp_record;
        temp_record->next = current;
        //fprintf(stderr,"Inserting before current\n");
      }

      //fprintf(stderr,"Adding:%d:%s\n",strlen(filename),filename);

      // Fill in some default values for the new record
//WE7U
// Here's where we might look at the file extension and assign
// default map_layer fields based on that.
      temp_record->max_zoom = 0;
      temp_record->min_zoom = 0;
      temp_record->map_layer = default_map_layer;
      temp_record->selected = 0;
      temp_record->XmStringPtr = NULL;

      if (       strstr(filename,".geo")
                 || strstr(filename,".GEO")
                 || strstr(filename,".Geo"))
      {
        temp_record->auto_maps = 0;
      }
      else
      {
        temp_record->auto_maps = 1;
      }

      if (       strstr(filename,".shp")
                 || strstr(filename,".SHP")
                 || strstr(filename,".Shp") )
      {
        temp_record->draw_filled = 2; // Auto
      }
      else
      {
        temp_record->draw_filled = 0; // No-Fill
      }

      if (       strstr(filename,".tif")
                 || strstr(filename,".TIF")
                 || strstr(filename,".Tif") )
      {
        temp_record->usgs_drg = 2; // Auto
      }
      else
      {
        temp_record->usgs_drg = 0; // No
      }

      //current = current->next;
      done++;
    }
    else    // Haven't gotten to the correct insertion point yet
    {
      previous = current; // Save ptr to last record
      current = current->next;
    }
  }

  if (!done)    // Matching record not found, add a
  {
    // record to the end of the list
    //fprintf(stderr,"Not Found: Adding an index record for %s\n",filename);
    temp_record = (map_index_record *)malloc(sizeof(map_index_record));
    CHECKMALLOC(temp_record);

    temp_record->next = NULL;

    if (previous == NULL)   // Empty list
    {
      map_index_head = temp_record;
      //fprintf(stderr,"First record in new list\n");
    }
    else    // Else at end of list
    {
      previous->next = temp_record;
      //fprintf(stderr,"Adding to end of list: %s\n",filename);
    }

    //fprintf(stderr,"Adding:%d:%s\n",strlen(filename),filename);

    // Fill in some default values for the new record
//WE7U
// Here's where we might look at the file extension and assign
// default map_layer fields based on that.
    temp_record->max_zoom = 0;
    temp_record->min_zoom = 0;
    temp_record->map_layer = default_map_layer;
    temp_record->selected = 0;
    temp_record->XmStringPtr = NULL;

    if (       strstr(filename,".geo")
               || strstr(filename,".GEO")
               || strstr(filename,".Geo"))
    {
      temp_record->auto_maps = 0;
    }
    else
    {
      temp_record->auto_maps = 1;
    }

    if (       strstr(filename,".shp")
               || strstr(filename,".SHP")
               || strstr(filename,".Shp") )
    {
      temp_record->draw_filled = 2; // Auto
    }
    else
    {
      temp_record->draw_filled = 0; // No-Fill
    }

    if (       strstr(filename,".tif")
               || strstr(filename,".TIF")
               || strstr(filename,".Tif") )
    {
      temp_record->usgs_drg = 2; // Auto
    }
    else
    {
      temp_record->usgs_drg = 0; // No
    }

  }

  // Update the values.  By this point we have a struct to fill
  // in, whether it's a new or old struct doesn't matter.  Convert
  // the values from lat/long to Xastir coordinate system.
  xastir_snprintf(temp_record->filename,MAX_FILENAME,"%s",filename);

  temp_record->bottom = bottom;
  temp_record->top = top;
  temp_record->left = left;
  temp_record->right = right;
  temp_record->accessed = 1;
}





// Function called by the various draw_* functions when in indexing
// mode.  Causes an update of the index list in memory.  Input
// parameters are in lat/long, which are converted to Xastir
// coordinates for storage due to speed considerations.  Records are
// inserted in alphanumerical order.
void index_update_ll(char *filename,
                     double bottom,
                     double top,
                     double left,
                     double right,
                     int default_map_layer)
{

  map_index_record *current = map_index_head;
  map_index_record *previous = map_index_head;
  map_index_record *temp_record = NULL;
  int done = 0;
  unsigned long temp_left, temp_right, temp_top, temp_bottom;
  int ok;
  int i;


  // Check for initial bad input
  if ( (filename == NULL)
       || (filename[0] == '\0')
       || (filename[strlen(filename) - 1] == '/') )
  {
    fprintf(stderr,"index_update_ll: Bad input: %s\n",filename);
    return;
  }
  // Make sure there aren't any weird characters in the filename
  // that might cause problems later.  Look for control characters
  // and convert them to string-end characters.
  for ( i = 0; i < (int)strlen(filename); i++ )
  {
    // Change any control characters to '\0' chars
    if (filename[i] < 0x20)
    {

      fprintf(stderr,"\nindex_update_ll: Found control char 0x%02x in map file/map directory name:\n%s\n",
              filename[i],
              filename);

      filename[i] = '\0';    // Terminate it here
    }
  }
  // Check if the string is _now_ bogus
  if (filename[0] == '\0')
  {
    fprintf(stderr,"index_update_ll: Bad input: %s\n",filename);
    return;
  }

  //fprintf(stderr,"index_update_ll: (%15.10g,%15.10g)\t(%15.10g,%15.10g)\t%s\n",
  //    bottom, top, left, right, filename );

  //if (map_index_head == NULL)
  //    fprintf(stderr,"Empty list\n");

  // Skip dbf and shx map extensions.  Really should make this
  // case-independent...
  if (       strstr(filename,"shx")
             || strstr(filename,"dbf")
             || strstr(filename,"SHX")
             || strstr(filename,"DBF") )
  {
    return;
  }

  // Search for a matching filename in the linked list
  while ((current != NULL) && !done)
  {
    int test;

    //fprintf(stderr,"Comparing %s to\n          %s\n",current->filename,filename);

    test = strcmp(current->filename,filename);

    if (test == 0)
    {
      // Found a match!
      //fprintf(stderr,"Found: Updating entry for %s\n",filename);
      temp_record = current;
      done++; // Exit the while loop
    }

    else if (test > 0)
    {
      // Found a string past us in the alphabet.  Insert ahead
      // of this last record.

      //fprintf(stderr,"\n%s\n%s\n",current->filename,filename);

      //fprintf(stderr,"Not Found: Inserting an index record for %s\n",filename);
      temp_record = (map_index_record *)malloc(sizeof(map_index_record));
      CHECKMALLOC(temp_record);

      if (current == map_index_head)    // Start of list!
      {
        // Insert new record at head of list
        temp_record->next = map_index_head;
        map_index_head = temp_record;
        //fprintf(stderr,"Inserting at head of list\n");
      }
      else
      {
        // Insert new record before "current"
        previous->next = temp_record;
        temp_record->next = current;
        //fprintf(stderr,"Inserting before current\n");
      }

      //fprintf(stderr,"Adding:%d:%s\n",strlen(filename),filename);

      // Fill in some default values for the new record
//WE7U
// Here's where we might look at the file extension and assign
// default map_layer fields based on that.
      temp_record->max_zoom = 0;
      temp_record->min_zoom = 0;
      temp_record->map_layer = default_map_layer;
      temp_record->selected = 0;
      temp_record->XmStringPtr = NULL;

      if (       strstr(filename,".geo")
                 || strstr(filename,".GEO")
                 || strstr(filename,".Geo"))
      {
        temp_record->auto_maps = 0;
      }
      else
      {
        temp_record->auto_maps = 1;
      }

      if (       strstr(filename,".shp")
                 || strstr(filename,".SHP")
                 || strstr(filename,".Shp") )
      {
        temp_record->draw_filled = 2; // Auto
      }
      else
      {
        temp_record->draw_filled = 0; // No-Fill
      }

      if (       strstr(filename,".tif")
                 || strstr(filename,".TIF")
                 || strstr(filename,".Tif") )
      {
        temp_record->usgs_drg = 2; // Auto
      }
      else
      {
        temp_record->usgs_drg = 0; // No
      }

      //current = current->next;
      done++;
    }
    else    // Haven't gotten to the correct insertion point yet
    {
      previous = current; // Save ptr to last record
      current = current->next;
    }
  }

  if (!done)      // Matching record not found, didn't find alpha
  {
    // chars after our string either, add record to
    // the end of the list.

    //fprintf(stderr,"Not Found: Adding an index record for %s\n",filename);
    temp_record = (map_index_record *)malloc(sizeof(map_index_record));
    CHECKMALLOC(temp_record);

    temp_record->next = NULL;

    if (previous == NULL)   // Empty list
    {
      map_index_head = temp_record;
      //fprintf(stderr,"First record in new list\n");
    }
    else    // Else at end of list
    {
      previous->next = temp_record;
      //fprintf(stderr,"Adding to end of list: %s\n",filename);
    }

    //fprintf(stderr,"Adding:%d:%s\n",strlen(filename),filename);

    // Fill in some default values for the new record
//WE7U
// Here's where we might look at the file extension and assign
// default map_layer fields based on that.
    temp_record->max_zoom = 0;
    temp_record->min_zoom = 0;
    temp_record->map_layer = default_map_layer;
    temp_record->selected = 0;
    temp_record->XmStringPtr = NULL;

    if (       strstr(filename,".geo")
               || strstr(filename,".GEO")
               || strstr(filename,".Geo"))
    {
      temp_record->auto_maps = 0;
    }
    else
    {
      temp_record->auto_maps = 1;
    }

    if (       strstr(filename,".shp")
               || strstr(filename,".SHP")
               || strstr(filename,".Shp") )
    {
      temp_record->draw_filled = 2; // Auto
    }
    else
    {
      temp_record->draw_filled = 0; // No-Fill
    }

    if (       strstr(filename,".tif")
               || strstr(filename,".TIF")
               || strstr(filename,".Tif") )
    {
      temp_record->usgs_drg = 2; // Auto
    }
    else
    {
      temp_record->usgs_drg = 0; // No
    }

  }

  // Update the values.  By this point we have a struct to fill
  // in, whether it's a new or old struct doesn't matter.  Convert
  // the values from lat/long to Xastir coordinate system.

  // In this case the struct uses MAX_FILENAME for the length of
  // the field, so the below statement is ok.
  xastir_snprintf(temp_record->filename,MAX_FILENAME,"%s",filename);

  ok = convert_to_xastir_coordinates( &temp_left,
                                      &temp_top,
                                      (float)left,
                                      (float)top);
  if (!ok)
  {
    fprintf(stderr,"%s\n\n",filename);
  }

  ok = convert_to_xastir_coordinates( &temp_right,
                                      &temp_bottom,
                                      (float)right,
                                      (float)bottom);
  if (!ok)
  {
    fprintf(stderr,"%s\n\n",filename);
  }

  temp_record->bottom = temp_bottom;
  temp_record->top = temp_top;
  temp_record->left = temp_left;
  temp_record->right = temp_right;
  temp_record->accessed = 1;
}





// Function which will update the "accessed" variable on either a
// directory or a filename in the map index.
static void index_update_accessed(char *filename)
{
  map_index_record *current = map_index_head;
  int done = 0;
  int i;


  // Check for initial bad input
  if ( (filename == NULL) || (filename[0] == '\0') )
  {
    fprintf(stderr,"index_update_accessed: Bad input: %s\n",filename);
    return;
  }

  // Make sure there aren't any weird characters in the filename
  // that might cause problems later.  Look for control characters
  // and convert them to string-end characters.
  for ( i = 0; i < (int)strlen(filename); i++ )
  {
    // Change any control characters to '\0' chars
    if (filename[i] < 0x20)
    {

      fprintf(stderr,"\nindex_update_accessed: Found control char 0x%02x in map file/map directory name:\n%s\n",
              filename[i],
              filename);

      filename[i] = '\0';    // Terminate it here
    }
  }
  // Check if the string is _now_ bogus
  if (filename[0] == '\0')
  {
    fprintf(stderr,"index_update_accessed: Bad input: %s\n",filename);
    return;
  }

  // Skip dbf and shx map extensions.  Really should make this
  // case-independent...
  if (       strstr(filename,"shx")
             || strstr(filename,"dbf")
             || strstr(filename,"SHX")
             || strstr(filename,"DBF") )
  {
    return;
  }

  // Search for a matching filename in the linked list
  while ((current != NULL) && !done)
  {
    int test;

//fprintf(stderr,"Comparing %s to\n          %s\n",current->filename,filename);

    test = strcmp(current->filename,filename);

    if (test == 0)
    {
      // Found a match!
//fprintf(stderr,"Found: Updating entry for %s\n\n",filename);
      current->accessed = 1;
      done++; // Exit the while loop
    }

    else    // Haven't gotten to the correct insertion point yet
    {
      current = current->next;
    }
  }
}





// Function called by map_onscreen_index()
//
// This function returns:
//      0 if the map isn't in the index
//      1 if the map is listed in the index
//      Four parameters listing the extents of the map
//
// The updated parameters are in the Xastir coordinate system for
// speed reasons.
//
// Note that the index retrieval could be made much faster by
// storing the data in a hash instead of a linked list.  This is
// just an initial implementation to see what speedups are possible.
// Hashing might be next.  --we7u
//
// Note that since we've alphanumerically ordered the list, we can
// stop when we hit something after this filename in the alphabet.
// It speeds things up quite a bit.
//
// In order to speed this up slightly for the general case, we'll
// assume that we'll be fetching indexes in alphabetical order, as
// that's how we store them everywhere.  We'll save the last map
// index pointer away and start searching there each time.  That
// should make all but the _first_ lookup much faster.
//
map_index_record *last_index_lookup = NULL;

int index_retrieve(char *filename,
                   unsigned long *bottom,
                   unsigned long *top,
                   unsigned long *left,
                   unsigned long *right,
                   int *max_zoom,
                   int *min_zoom,
                   int *map_layer,
                   int *draw_filled,
                   int *usgs_drg,
                   int *auto_maps)
{

  map_index_record *current;
  int status = 0;


  if ( (filename == NULL)
       || (strlen(filename) >= MAX_FILENAME) )
  {
    return(status);
  }

  // Attempt to start where we left off last time
  if (last_index_lookup != NULL)
  {
    current = last_index_lookup;
  }
  else
  {
    current = map_index_head;
//fprintf(stderr,"Start at beginning:%s\t", filename);
  }

  // Check to see if we're past the correct area.  If so, start at
  // the beginning of the index instead.
  //
  if (current
      && ((current->filename[0] > filename[0])
          || (strcmp(current->filename, filename) > 0)))
  {
    //
    // We're past the correct point.  Start at the beginning of
    // the list unless we're already there.
    //
    if (current != map_index_head)
    {
      current = map_index_head;
    }
//fprintf(stderr,"Start at beginning:%s\t", filename);
  }

  //
  // Search for a matching filename in the linked list.
  //

  // Check the first char only.  Loop until they match or go past.
  // This is our high-speed method to get to the correct search
  // area.
  //
  while (current && (current->filename[0] < filename[0]))
  {
    // Save the pointer away for next time.  There's a reason we
    // save it before we increment the counter:  For "z" weather
    // alerts, it's nice to have it scan just the very last of
    // the list before it fails, instead of scanning the entire
    // list each time and then failing.  Need to find out why
    // weather alerts always fail, and therefore why this
    // routine gets called every time for them.
    //
    last_index_lookup = current;
    current = current->next;
//fprintf(stderr,"1");
  }

  // Stay in this loop while the first char matches.  This is our
  // active search area.
  //
  while (current && (current->filename[0] == filename[0]))
  {
    int result;

    // Check the entire string
    result = strcmp(current->filename, filename);

    if (result == 0)
    {
      // Found a match!
      status = 1;
      *bottom = current->bottom;
      *top = current->top;
      *left = current->left;
      *right = current->right;
      *max_zoom = current->max_zoom;
      *min_zoom = current->min_zoom;
      *map_layer = current->map_layer;
      *draw_filled = current->draw_filled;
      *usgs_drg = current->usgs_drg;
      *auto_maps = current->auto_maps;
//fprintf(stderr," Found it\n");
      return(status);
    }
    else if (result > 0)
    {
      // We're past it in the index.  We didn't find it in the
      // index.
//fprintf(stderr," Did not find1\n");
      return(status);
    }
    else    // Not found yet, look at the next
    {
      // Save the pointer away for next time.  There's a
      // reason we save it before we increment the counter:
      // For "z" weather alerts, it's nice to have it scan
      // just the very last of the list before it fails,
      // instead of scanning the entire list each time and
      // then failing.  Need to find out why weather alerts
      // always fail, and therefore why this routine gets
      // called every time for them.
      //
      last_index_lookup = current;
      current = current->next;
//fprintf(stderr,"2");
    }
  }

  // We're past the correct search area and didn't find it.
//fprintf(stderr," Did not find2\n");

  return(status);
}





// Saves the linked list pointed to by map_index_head to a file.
// Keeps the same order as the memory linked list.  Delete records
// in the in-memory linked list for which the "accessed" variable is
// 0 or filename is empty.
//
void index_save_to_file(void)
{
  FILE *f;
  map_index_record *current;
//  map_index_record *last;
  char out_string[MAX_FILENAME*2];
  char map_index_path[MAX_VALUE];

  get_user_base_dir(MAP_INDEX_DATA, map_index_path, sizeof(map_index_path));


//fprintf(stderr,"Saving map index to file\n");

  f = fopen( map_index_path, "w" );

  if (f == NULL)
  {
    fprintf(stderr,"Couldn't create/update map index file: %s\n",
            map_index_path );
    return;
  }

  current = map_index_head;
//  last = current;

  while (current != NULL)
  {
    int i;

    // Make sure there aren't any weird characters in the
    // filename that might cause problems later.  Look for
    // control characters and convert them to string-end
    // characters.
    for ( i = 0; i < (int)strlen(current->filename); i++ )
    {
      // Change any control characters to '\0' chars
      if (current->filename[i] < 0x20)
      {

        fprintf(stderr,"\nindex_save_to_file: Found control char 0x%02x in map name:\n%s\n",
                current->filename[i],
                current->filename);

        current->filename[i] = '\0';    // Terminate it here
      }
    }

    // Save to file if filename non-blank and record has the
    // accessed field set.
    if ( (current->filename[0] != '\0')
         && (current->accessed != 0) )
    {

      // Write each object out to the file as one
      // comma-delimited line
      xastir_snprintf(out_string,
                      sizeof(out_string),
                      "%010lu,%010lu,%010lu,%010lu,%05d,%01d,%01d,%01d,%05d,%05d,%s\n",
                      current->bottom,
                      current->top,
                      current->left,
                      current->right,
                      current->map_layer,
                      current->draw_filled,
                      current->usgs_drg,
                      current->auto_maps,
                      current->max_zoom,
                      current->min_zoom,
                      current->filename);

      if (fprintf(f,"%s",out_string) < (int)strlen(out_string))
      {
        // Failed to write
        fprintf(stderr,"Couldn't write objects to map index file: %s\n",
                map_index_path );
        current = NULL; // All done
      }
      // Set up pointers for next loop iteration
//          last = current;

      if (current != NULL)
      {
        current = current->next;
      }
    }


    else
    {
//          last = current;
      current = current->next;
    }
    /*
    //WE7U
            else {  // Delete this record from our list!  It's a record
                    // for a map file that doesn't exist in the
                    // filesystem anymore.
                if (last == current) {   // We're at the head of the list
                    map_index_head = current->next;

    // Remember to free the XmStringPtr if we use this bit of code
    // again.

                    free(current);

                    // Set up pointers for next loop iteration
                    current = map_index_head;
                    last = current;
                }
                else {  // Not the first record in the list
                    map_index_record *gone;

                    gone = current; // Save ptr to record we wish to delete
                    last->next = current->next; // Unlink from list

    // Remember to free the XmStringPtr if we use this bit of code
    // again.

                    free(gone);

                    // Set up pointers for next loop iteration
                    // "last" is still ok
                    current = last->next;
                }
            }
    */
  }
  (void)fclose(f);
}





// This function is currently not used.
//
// Function used to add map directories/files to the in-memory map
// index.  Causes an update of the index list in memory.  Input
// records are inserted in alphanumerical order.  This function is
// called from the index_restore_from_file() function below.  When
// this function is called the new record has all of the needed
// information in it.
//
/*
static void index_insert_sorted(map_index_record *new_record) {

    map_index_record *current = map_index_head;
    map_index_record *previous = map_index_head;
    int done = 0;
    int i;


    //fprintf(stderr,"index_insert_sorted: %s\n", new_record->filename );

    // Check for bad input.
    if (new_record == NULL) {
        fprintf(stderr,"index_insert_sorted: Bad input.\n");
        return;
    }
    // Make sure there aren't any weird characters in the filename
    // that might cause problems later.  Look for any control
    // characters and convert them to string-end characters.
    for ( i = 0; i < (int)strlen(new_record->filename); i++ ) {
        if (new_record->filename[i] < 0x20) {

            fprintf(stderr,"\nindex_insert_sorted: Found control char 0x%02x in map name:\n%s\n",
                new_record->filename[i],
                new_record->filename);

            new_record->filename[i] = '\0';    // Terminate it here
        }
    }
    // Check if the string is _now_ bogus
    if (new_record->filename[0] == '\0') {
        fprintf(stderr,"index_insert_sorted: Bad input.\n");
        return;
    }

    //if (map_index_head == NULL)
    //    fprintf(stderr,"Empty list\n");

    // Search for a matching filename in the linked list
    while ((current != NULL) && !done) {
        int test;

        //fprintf(stderr,"Comparing %s to\n          %s\n",
        //    current->filename, new_record->filename);

        test = strcmp(current->filename, new_record->filename);

        if (test == 0) {    // Found a match!
            int selected;

//fprintf(stderr,"Found a match: Updating entry for %s\n",new_record->filename);

            // Save this away temporarily.
            selected = current->selected;

            // Copy the fields across and then free new_record.  We
            // overwrite the contents of the existing record.
            xastir_snprintf(current->filename,
                MAX_FILENAME,
                "%s",
                new_record->filename);
            current->bottom = new_record->bottom;
            current->top = new_record->top;
            current->left = new_record->left;
            current->right = new_record->right;
            current->accessed = 1;
            current->max_zoom = new_record->max_zoom;
            current->min_zoom = new_record->min_zoom;
            current->map_layer = new_record->map_layer;
            current->draw_filled = new_record->draw_filled;
            current->usgs_drg = new_record->usgs_drg;
            current->selected = selected;   // Restore it
            current->auto_maps = new_record->auto_maps;

// Remember to free the XmStringPtr if we use this bit of code
// again.

            free(new_record);   // Don't need it anymore

            done++; // Exit loop, "current" points to found record
        }
        else if (test > 0) {    // Found a string past us in the
                                // alphabet.  Insert ahead of this
                                // last record.

//fprintf(stderr,"Not Found, inserting: %s\n", new_record->filename);
//fprintf(stderr,"       Before record: %s\n", current->filename);

            if (current == map_index_head) {  // Start of list!
                // Insert new record at head of list
                new_record->next = map_index_head;
                map_index_head = new_record;
                //fprintf(stderr,"Inserting at head of list\n");
            }
            else {  // Insert between "previous" and "current"
                // Insert new record before "current"
                previous->next = new_record;
                new_record->next = current;
                //fprintf(stderr,"Inserting before current\n");
            }

            //fprintf(stderr,"Adding:%d:%s\n",strlen(filename),filename);

            // Fill in some default values for the new record that
            // don't exist in the map_index.sys file.
            new_record->selected = 0;

            if (       strstr(new_record->filename,".geo")
                    || strstr(new_record->filename,".GEO")
                    || strstr(new_record->filename,".Geo") ) {
                new_record->auto_maps = 0;
            }
            else {
                new_record->auto_maps = 1;
            }

            //current = current->next;
            done++;
        }
        else {  // Haven't gotten to the correct insertion point yet
            previous = current; // Save ptr to last record
            current = current->next;
        }
    }

    if (!done) {    // Matching record not found, add the record to
        // the end of the list.  "previous" points to the last
        // record in the list or NULL (empty list).

//fprintf(stderr,"Not Found: Adding to end: %s\n",new_record->filename);

        new_record->next = NULL;

        if (previous == NULL) { // Empty list
            map_index_head = new_record;
            //fprintf(stderr,"First record in new list\n");
        }
        else {  // Else at end of list
            previous->next = new_record;
            //fprintf(stderr,"Adding to end of list: %s\n",new_record->filename);
        }

        //fprintf(stderr,"Adding:%d:%s\n",strlen(new_record->filename),new_record->filename);

        // Fill in some default values for the new record.
        new_record->selected = 0;

        if (       strstr(new_record->filename,".geo")
                || strstr(new_record->filename,".GEO")
                || strstr(new_record->filename,".Geo") ) {
            new_record->auto_maps = 0;
        }
        else {
            new_record->auto_maps = 1;
        }
    }
}
*/





// sort map index
// simple bubble sort, since we should be sorted already
//
static void index_sort(void)
{
  map_index_record *current, *previous, *next;
  int changed = 1;
  int loops = 0; // for debug stats

  previous = map_index_head;
  next = NULL;
  //  fprintf(stderr, "index_sort: start.\n");
  // check if we have any records at all, and at least two
  if ( (previous != NULL) && (previous->next != NULL) )
  {
    current = previous->next;
    while ( changed == 1)
    {
      changed = 0;
      if (current->next != NULL)
      {
        next = current->next;
      }
      if ( strcmp( previous->filename, current->filename) >= 0 )
      {
        // out of order - swap them
        current->next = previous;
        previous->next = next;
        map_index_head = current;
        current = previous;
        previous = map_index_head;
        changed = 1;
      }

      while ( next != NULL )
      {
        if ( strcmp( current->filename, next->filename) >= 0 )
        {
          // out of order - swap them
          current->next = next->next;
          previous->next = next;
          next->next = current;
          // get ready for the next iteration
          previous = next;  // current already moved ahead from the swap
          next = current->next;
          changed = 1;
        }
        else
        {
          previous = current;
          current = next;
          next = current->next;
        }
      }
      previous = map_index_head;
      current = previous->next;
      next = current->next;
      loops++;
    }
  }
  // debug stats
  // fprintf(stderr, "index_sort: ran %d loops.\n", loops);
}





// Snags the file and creates the linked list pointed to by the
// map_index_head pointer.  The memory linked list keeps the same
// order as the entries in the file.
//
// NOTE:  If we're converting from the old format to the new, we
// need to call index_save_to_file() in order to write out the new
// format once we're done.
//
void index_restore_from_file(void)
{
  FILE *f;
  map_index_record *temp_record;
  map_index_record *last_record;
  char in_string[MAX_FILENAME*2];
  int doing_migration = 0;
  char map_index_path[MAX_VALUE];

  get_user_base_dir(MAP_INDEX_DATA, map_index_path, sizeof(map_index_path));


//fprintf(stderr,"\nRestoring map index from file\n");

  if (map_index_head != NULL)
  {
    fprintf(stderr,"Warning: index_restore_from_file(): map_index_head was non-null!\n");
  }

  map_index_head = NULL;  // Starting with empty list
  last_record = NULL;

  f = fopen( map_index_path, "r" );
  if (f == NULL)  // No map_index file yet
  {
    return;
  }

  while (!feof (f))   // Loop through entire map_index file
  {

    // Read one line from the file
    if ( get_line (f, in_string, MAX_FILENAME*2) )
    {

      if (strlen(in_string) >= 15)     // We have some data.
      {
        // Try to process the
        // line.
        char scanf_format[50];
        char old_scanf_format[50];
        char older_scanf_format[50];
        int processed;
        int i, jj;

//fprintf(stderr,"%s\n",in_string);

        // Tweaked the string below so that it will track
        // along with MAX_FILENAME-1.  We're constructing
        // the string "%lu,%lu,%lu,%lu,%d,%d,%2000c", where
        // the 2000 example number is from MAX_FILENAME.
        xastir_snprintf(scanf_format,
                        sizeof(scanf_format),
                        "%s%d%s",
                        "%lu,%lu,%lu,%lu,%d,%d,%d,%d,%d,%d,%",
                        MAX_FILENAME,
                        "c");
        //fprintf(stderr,"%s\n",scanf_format);

        // index predates addition of usgs_drg flag (26 Jul 2005)
        xastir_snprintf(old_scanf_format,
                        sizeof(old_scanf_format),
                        "%s%d%s",
                        "%lu,%lu,%lu,%lu,%d,%d,%d,%d,%d,%",
                        MAX_FILENAME,
                        "c");

        // index predates addition of min/max zoom (29 Oct 2003)
        xastir_snprintf(older_scanf_format,
                        sizeof(older_scanf_format),
                        "%s%d%s",
                        "%lu,%lu,%lu,%lu,%d,%d,%d,%",
                        MAX_FILENAME,
                        "c");

        // Malloc an index record.  We'll add it to the list
        // only if the data looks reasonable.
        temp_record = (map_index_record *)malloc(sizeof(map_index_record));
        CHECKMALLOC(temp_record);

        memset(temp_record->filename, 0, sizeof(temp_record->filename));
        temp_record->next = NULL;
        temp_record->bottom = 64800001l;// Too high
        temp_record->top = 64800001l;   // Too high
        temp_record->left = 129600001l; // Too high
        temp_record->right = 129600001l;// Too high
        temp_record->map_layer = -1;    // Too low
        temp_record->draw_filled = -1;  // Too low
        temp_record->usgs_drg = -1;     // Too low
        temp_record->auto_maps = -1;    // Too low
        temp_record->max_zoom = -1;     // Too low
        temp_record->min_zoom = -1;     // Too low
        temp_record->filename[0] = '\0';// Empty

        processed = sscanf(in_string,
                           scanf_format,
                           &temp_record->bottom,
                           &temp_record->top,
                           &temp_record->left,
                           &temp_record->right,
                           &temp_record->map_layer,
                           &temp_record->draw_filled,
                           &temp_record->usgs_drg,
                           &temp_record->auto_maps,
                           &temp_record->max_zoom,
                           &temp_record->min_zoom,
                           temp_record->filename);

        if (processed < 11)
        {
          // We're upgrading from an old format index file
          // that doesn't have usgs_drg.  Try the
          // old_scanf_format string instead.

          doing_migration = 1;

          processed = sscanf(in_string,
                             old_scanf_format,
                             &temp_record->bottom,
                             &temp_record->top,
                             &temp_record->left,
                             &temp_record->right,
                             &temp_record->map_layer,
                             &temp_record->draw_filled,
                             &temp_record->auto_maps,
                             &temp_record->max_zoom,
                             &temp_record->min_zoom,
                             temp_record->filename);
          if (processed < 10)
          {
            // It's really old, doesn't have min/max zoom either
            temp_record->max_zoom = -1;     // Too low
            temp_record->min_zoom = -1;     // Too low

            processed = sscanf(in_string,
                               older_scanf_format,
                               &temp_record->bottom,
                               &temp_record->top,
                               &temp_record->left,
                               &temp_record->right,
                               &temp_record->map_layer,
                               &temp_record->draw_filled,
                               &temp_record->auto_maps,
                               temp_record->filename);
          }
          // either way, it doesn't have usgs_drg, so add one
          // defaulting to Auto if it's a tif file, no if not
          if (       strstr(temp_record->filename,".tif")
                     || strstr(temp_record->filename,".TIF")
                     || strstr(temp_record->filename,".Tif") )
          {
            temp_record->usgs_drg = 2; // Auto
          }
          else
          {
            temp_record->usgs_drg = 0; // No
          }
        }

        temp_record->XmStringPtr = NULL;

        // Do some reasonableness checking on the parameters
        // we just parsed.
//WE7U: First comparison here is always false
//                if ( (temp_record->bottom < 0l)
//                        || (temp_record->bottom > 64800000l) ) {
        if (temp_record->bottom > 64800000l)
        {

          processed = 0;  // Reject this record
          fprintf(stderr,"\nindex_restore_from_file: bottom extent incorrect %lu in map name:\n%s\n",
                  temp_record->bottom,
                  temp_record->filename);
        }


//WE7U: First comparison here is always false
//               if ( (temp_record->top < 0l)
//                        || (temp_record->top > 64800000l) ) {
        if (temp_record->top > 64800000l)
        {

          processed = 0;  // Reject this record
          fprintf(stderr,"\nindex_restore_from_file: top extent incorrect %lu in map name:\n%s\n",
                  temp_record->top,
                  temp_record->filename);
        }

//WE7U: First comparison here is always false
//                if ( (temp_record->left < 0l)
//                        || (temp_record->left > 129600000l) ) {
        if (temp_record->left > 129600000l)
        {

          processed = 0;  // Reject this record
          fprintf(stderr,"\nindex_restore_from_file: left extent incorrect %lu in map name:\n%s\n",
                  temp_record->left,
                  temp_record->filename);
        }

//WE7U: First comparison here is always false
//                if ( (temp_record->right < 0l)
//                        || (temp_record->right > 129600000l) ) {
        if (temp_record->right > 129600000l)
        {

          processed = 0;  // Reject this record
          fprintf(stderr,"\nindex_restore_from_file: right extent incorrect %lu in map name:\n%s\n",
                  temp_record->right,
                  temp_record->filename);
        }

        if ( (temp_record->max_zoom < 0)
             || (temp_record->max_zoom > 99999) )
        {
//                    processed = 0;  // Reject this record
//                    fprintf(stderr,"\nindex_restore_from_file: max_zoom field incorrect %d in map name:\n%s\n",
//                            temp_record->max_zoom,
//                            temp_record->filename);
          // Assign a reasonable value
          temp_record->max_zoom = 0;
          //fprintf(stderr,"Assigning max_zoom of 0\n");
        }

        if ( (temp_record->min_zoom < 0)
             || (temp_record->min_zoom > 99999) )
        {
//                    processed = 0;  // Reject this record
//                    fprintf(stderr,"\nindex_restore_from_file: min_zoom field incorrect %d in map name:\n%s\n",
//                            temp_record->min_zoom,
//                            temp_record->filename);
          // Assign a reasonable value
          temp_record->min_zoom = 0;
          //fprintf(stderr,"Assigning min_zoom of 0\n");
        }

        if ( (temp_record->map_layer < -99999)
             || (temp_record->map_layer > 99999) )
        {
          processed = 0;  // Reject this record
          fprintf(stderr,"\nindex_restore_from_file: map_layer field incorrect %d in map name:\n%s\n",
                  temp_record->map_layer,
                  temp_record->filename);
        }

        if ( (temp_record->draw_filled < 0)
             || (temp_record->draw_filled > 2) )
        {
          processed = 0;  // Reject this record
          fprintf(stderr,"\nindex_restore_from_file: draw_filled field incorrect %d in map name:\n%s\n",
                  temp_record->draw_filled,
                  temp_record->filename);
        }

        if ( (temp_record->usgs_drg < 0)
             || (temp_record->usgs_drg > 2) )
        {
          processed = 0;  // Reject this record
          fprintf(stderr,"\nindex_restore_from_file: usgs_drg field incorrect %d in map name:\n%s\n",
                  temp_record->usgs_drg,
                  temp_record->filename);
        }

        if ( (temp_record->auto_maps < 0)
             || (temp_record->auto_maps > 1) )
        {
          processed = 0;  // Reject this record
          fprintf(stderr,"\nindex_restore_from_file: auto_maps field incorrect %d in map name:\n%s\n",
                  temp_record->auto_maps,
                  temp_record->filename);
        }

        // Check whether the filename is empty
        if (strlen(temp_record->filename) == 0)
        {
          processed = 0;  // Reject this record
        }

        // Check for control characters in the filename.
        // Reject any that have them.
        jj = (int)strlen(temp_record->filename);
        for (i = 0; i < jj; i++)
        {
          if (temp_record->filename[i] < 0x20)
          {

            processed = 0;  // Reject this record
            fprintf(stderr,"\nindex_restore_from_file: Found control char 0x%02x in map name:\n%s\n",
                    temp_record->filename[i],
                    temp_record->filename);
          }
        }


        // Mark the record as accessed at this point.
        // At the stage where we're writing this list off to
        // disk, if the record hasn't been accessed by the
        // re-indexing, it doesn't get written.  This has
        // the effect of flushes deleted files from the
        // index quickly.
        temp_record->accessed = 1;

        // Default is not-selected.  Later we read in the
        // selected_maps.sys file and tweak some of these
        // fields.
        temp_record->selected = 0;

        temp_record->filename[MAX_FILENAME-1] = '\0';

        // If correct number of parameters for either old or
        // new format
        if (processed == 11 || processed == 10 || processed == 8)
        {

          //fprintf(stderr,"Restored: %s\n",temp_record->filename);

          // Insert the new record into the in-memory map
          // list in sorted order.
          // --slow for large lists
          // index_insert_sorted(temp_record);
          // -- so we just add it to the end of the list
          // and sort it at the end tp make sure nobody
          // messed us up by editting the file by hand
          if ( last_record == NULL )   // first record
          {
            map_index_head = temp_record;
          }
          else
          {
            last_record->next = temp_record;
          }
          last_record = temp_record;


          // Remember that we may just have attached the
          // record to our in-memory map list, or we may
          // have free'ed it in the above function call.
          // Set the pointer to NULL to make sure we don't
          // try to do anything else with the memory.
          temp_record = NULL;
        }
        else    // sscanf didn't parse the proper number of
        {
          // items.  Delete the record.

// Remember to free the XmString pointer if necessary.

          free(temp_record);
//                    fprintf(stderr,"index_restore_from_file:sscanf parsing error\n");
        }
      }
    }
  }
  (void)fclose(f);
  // now that we have read the whole file, make sure it is sorted
  index_sort();  // probably should check for dup records

  if (doing_migration)
  {
    // Save in new file format if we just did a migration from
    // old format to new.
    fprintf(stderr,"Migrating from old map_index.sys format to new format.\n");
    index_save_to_file();
  }
}





// map_indexer()
//
// Recurses through the map directories finding map extents
// and recording them in the map index.  Once the indexing is
// complete, write the current index out to a file.
//
// It'd be nice to call index_restore_from_file() from main.c:main()
// so that an earlier copy of the index is restored before the map
// display is created.
//
// If we set the "accessed" variable in the in-memory index to 0 for
// each record and then run the indexer, the save-to-file function
// will delete those with a value of 0 when writing to disk.  Those
// maps no longer exist in the filesystem and should be deleted.  We
// could either wipe them from the in-memory database at that time
// as well, or wipe the whole list and re-read it from disk to get
// the current list.
//
// If parameter is 0, we'll do the smart timestamp-checking
// indexing.
// If 1, we'll erase the in-memory index and do full indexing.
//
void map_indexer(int parameter)
{
  struct stat nfile;
  int check_times = 1;
  FILE *f;
  map_index_record *current;
  map_index_record *backup_list_head = NULL;
  char map_index_path[MAX_VALUE];

  get_user_base_dir(MAP_INDEX_DATA, map_index_path, sizeof(map_index_path));


  if (debug_level & 16)
  {
    fprintf(stderr,"map_indexer() start\n");
  }

  fprintf(stderr,"Indexing maps...\n");

#ifdef HAVE_LIBSHP
#ifdef WITH_DBFAWK
  // get rid of stored dbfawk signatures and force reload.
  clear_dbfawk_sigs();
#endif
#endif

  // Find the timestamp on the index file first.  Save it away so
  // that the timestamp for each map file can be compared to it.
  if (stat ( map_index_path, &nfile) != 0)
  {

    // File doesn't exist yet.  Create it.
    f = fopen( map_index_path, "w" );
    if (f != NULL)
    {
      (void)fclose(f);
    }
    else
      fprintf(stderr,"Couldn't create map index file: %s\n",
              map_index_path );

    check_times = 0; // Don't check the timestamps.  Do them all.
  }
  else    // File exists
  {
    map_index_timestamp = (time_t)nfile.st_mtime;
    check_times = 1;
  }


  if (parameter == 1)     // Full indexing instead of timestamp-check indexing
  {

    // Move the in-memory index to a backup pointer
    backup_list_head = map_index_head;
    map_index_head = NULL;

//        // Set the timestamp to 0 so that everything gets indexed
//        map_index_timestamp = (time_t)0l;

    check_times = 0;
  }


  // Set the "accessed" field to zero for every record in the
  // index.  Note that the list could be empty at this point.
  current = map_index_head;
  while (current != NULL)
  {
    current->accessed = 0;
    current = current->next;
  }


  if (check_times)
  {
    if (debug_level & 16)
    {
      fprintf(stderr,"map_indexer: Calling map_search\n");
    }

    map_search (NULL, AUTO_MAP_DIR, NULL, NULL, (int)FALSE, INDEX_CHECK_TIMESTAMPS);

    if (debug_level & 16)
    {
      fprintf(stderr,"map_indexer: Returned from map_search\n");
    }
  }
  else
  {
    if (debug_level & 16)
    {
      fprintf(stderr,"map_indexer: Calling map_search\n");
    }

    map_search (NULL, AUTO_MAP_DIR, NULL, NULL, (int)FALSE, INDEX_NO_TIMESTAMPS);

    if (debug_level & 16)
    {
      fprintf(stderr,"map_indexer: Returned from map_search\n");
    }
  }

  if (debug_level & 16)
  {
    fprintf(stderr,"map_indexer() middle\n");
  }


  if (parameter == 1)     // Full indexing instead of timestamp-check indexing
  {
    // Copy the Properties from the backup list to the new list,
    // then free the backup list.
    map_index_copy_properties(map_index_head, backup_list_head);
  }


  // Save the updated index to the file
  index_save_to_file();

  fprintf(stderr,"Finished indexing maps\n");

  if (debug_level & 16)
  {
    fprintf(stderr,"map_indexer() end\n");
  }
}





/* moved these here and made them static so it will function on FREEBSD */
#define MAX_ALERT 7000
// If we comment this out, we link, but get a segfault at runtime.
// Take out the "static" and we get a segfault when we zoom out too
// far with the lakes or counties shapefile loaded.  No idea why
// yet.  --we7u
//static alert_entry alert[MAX_ALERT];
static int alert_count;





/*******************************************************************
 * fill_in_new_alert_entries()
 *
 * Fills in the index and filename portions of any alert entries
 * that are missing them.  This function should be called at the
 * point where we've just received a new weather alert.
 *

//WE7U
// Later we should change this so that it doesn't scan the entire
// message list, but is passed the important info directly from the
// decode routines in db.c, and the message should NOT be added to
// the message list.
//WE7U

 *
 * This function is designed to use ESRI Shapefile map files.  The
 * base directory where the Shapefiles are located is passed to us
 * in the "dir" variable.
 *
 * map_search() fills in the filename field of the alert struct.
 * draw_shapefile_map() fills in the index field.
 *******************************************************************/
void fill_in_new_alert_entries()
{
//  int ii;
  char alert_scan[MAX_FILENAME];
//  char *dir_ptr;
  struct hashtable_itr *iterator = NULL;
  alert_entry *temp;
  char dir[MAX_FILENAME];


  if (debug_level & 2)
  {
    fprintf(stderr,"fill_in_new_alert_entries start\n");
  }

  xastir_snprintf(dir,
                  sizeof(dir),
                  "%s",
                  ALERT_MAP_DIR);

  alert_count = MAX_ALERT - 1;

  // Set up our path to the wx alert maps
  memset(alert_scan, 0, sizeof (alert_scan));    // Zero our alert_scan string
  xastir_snprintf(alert_scan, // Fetch the base directory
                  sizeof(alert_scan),
                  "%s",
                  dir);
  strncat(alert_scan, // Complete alert directory is now set up in the string
          "/",
          sizeof(alert_scan) - 1 - strlen(alert_scan));
//  dir_ptr = &alert_scan[strlen (alert_scan)]; // Point to end of path

  // Iterate through the weather alerts.  It looks like we wish to
  // just fill in the alert struct and to determine whether the
  // alert is within our viewport here.  We don't wish to draw the
  // alerts at this stage, that happens in the load_alert_maps()
  // function below.

  iterator = create_wx_alert_iterator();
  temp = get_next_wx_alert(iterator);
  while (iterator != NULL && temp)
  {

    if (!temp->filename[0])   // Filename is
    {
      // empty, we need to fill it in.

//            fprintf(stderr,"fill_in_new_alert_entries() Title: %s\n",temp->title);

      // The last parameter denotes loading into
      // pixmap_alerts instead of pixmap or pixmap_final.
      // Note that just calling map_search does not get
      // the alert areas drawn on the screen.  The
      // draw_map() function called by map_search just
      // fills in the filename field in the struct and
      // exits.
      //
      // The "warn" parameter (next to last) specifies whether
      // to dump warnings out to the console as well.  If the
      // warning was received on local RF or locally, warn the
      // operator (the weather must be near).
      map_search (da,
                  alert_scan,
                  temp,
                  &alert_count,
                  (int)temp->flags[source],
                  DRAW_TO_PIXMAP_ALERTS);

//            fprintf(stderr,"fill_in_new_alert_entries() Title1:%s\n",temp->title);
    }
    temp = get_next_wx_alert(iterator);
  }
#ifndef USING_LIBGC
//fprintf(stderr,"free iterator 4\n");
  if (iterator)
  {
    free(iterator);
  }
#endif  // USING_LIBGC

  if (debug_level & 2)
  {
    fprintf(stderr,"fill_in_new_alert_entries end\n");
  }
}




/*******************************************************************
 * load_alert_maps()
 *
 * Used to load weather alert maps, based on NWS weather alerts that
 * are received.  Called from create_image() and refresh_image().
 * This function is designed to use ESRI Shapefile map files.  The
 * base directory where the Shapefiles are located is passed to us
 * in the "dir" variable.
 *
 * map_search() fills in the filename field of the alert struct.
 * draw_shapefile_map() fills in the index field.
 *******************************************************************/
void load_alert_maps (Widget w, char *dir)
{
//    int ii;
  int level;
  unsigned char fill_color[] = {  (unsigned char)0x69,    // gray86
                                  (unsigned char)0x4a,    // red2
                                  (unsigned char)0x63,    // yellow2
                                  (unsigned char)0x66,    // cyan2
                                  (unsigned char)0x61,    // RoyalBlue
                                  (unsigned char)0x64,    // ForestGreen
                                  (unsigned char)0x62
                               };  // orange3

  struct hashtable_itr *iterator = NULL;
  alert_entry *temp;
  map_draw_flags mdf;


//fprintf(stderr,"load_alert_maps\n");

// TODO:
// Figure out how to pass a quantity of zones off to the map drawing
// routines, then we can draw them all with one pass through each
// map file.  Alphanumerically sort the zones to make it easier for
// the map drawing functions?  Note that the indexing routines fill
// in both the filename and the shapefile index for each record.
//
// Alternative:  Call map_draw for each filename listed and have the
// draw_shapefile function iterate through the array looking for all
// filename matches, pulling non-negative indexes out of each index
// field for matches and drawing them.  That should be fast and
// require no sorting of the array.  Downside:  The alerts won't be
// layered based on alert level unless we modify the above:  Drawing
// each file once for each alert-level in the proper layering order.
// Perhaps we could keep a list of which filenames have been called,
// and only call each one once per load_alert_maps() call.


// Just for a test
//draw_shapefile_map (w, dir, filenm, alert, alert_color, destination_pixmap);
//draw_shapefile_map (w, dir, "c_16my01.shp", NULL, '\0', DRAW_TO_PIXMAP_ALERTS);


// Are we drawing them in reverse order so that the important
// alerts end up drawn on top of the less important alerts?
// Actually, since the alert hash isn't ordered, perhaps we need to
// order them by priority, then by map file, so that we can draw the
// shapes from each map file in the correct order.  This might cause
// each map file to be drawn up to three times (once for each
// priority level), but that's better than calling each map for each
// zone as is done now.

  iterator = create_wx_alert_iterator();
  temp = get_next_wx_alert(iterator);
  while (iterator != NULL && temp)
  {

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
    {
#ifndef USING_LIBGC
//fprintf(stderr,"free iterator 5\n");
      if (iterator)
      {
        free(iterator);
      }
#endif  // USING_LIBGC
      return;
    }

    if (disable_all_maps)
    {
#ifndef USING_LIBGC
//fprintf(stderr,"free iterator 6\n");
      if (iterator)
      {
        free(iterator);
      }
#endif  // USING_LIBGC
      return;
    }

    //  Check whether the alert slot is filled/empty
    if (temp->title[0] == '\0')   // Empty slot
    {
      temp = get_next_wx_alert(iterator);
      continue;
    }

    if ( (level = alert_active(temp, ALERT_ALL) ) )
    {
      if (level >= (int)sizeof (fill_color))
      {
        level = 0;
      }

      // The last parameter denotes drawing into pixmap_alert
      // instead of pixmap or pixmap_final.

      if (debug_level & 16)
      {
        fprintf(stderr,"load_alert_maps() Drawing %s\n",temp->filename);
        fprintf(stderr,"load_alert_maps() Title4:%s\n",temp->title);
      }

      // Attempt to draw alert
      if ( temp->index != -1 )            // Shape found in shapefile
      {

        // Check whether we've ever tried to draw this alert
        // before.  If not, attempt it and get the boundary
        // limits filled in.
        //
        if (       temp->bottom_boundary == 0.0
                   && temp->top_boundary == 0.0
                   && temp->left_boundary == 0.0
                   && temp->right_boundary == 0.0)
        {

          if (temp->alert_level != 'C')
          {
            draw_map (w, dir, temp->filename, temp,
                      fill_color[level], DRAW_TO_PIXMAP_ALERTS, &mdf);  // draw filled
          }
        }

        if (map_visible_lat_lon(temp->bottom_boundary, // Shape visible
                                temp->top_boundary,
                                temp->left_boundary,
                                temp->right_boundary) )
        {

          if (temp->alert_level != 'C')       // Alert not cancelled
          {
            mdf.draw_filled=1;
            mdf.usgs_drg=0;

            if (debug_level & 16)
            {
              fprintf(stderr,"load_alert_maps: Calling draw_map\n");
            }

            draw_map (w, dir, temp->filename, temp,
                      fill_color[level], DRAW_TO_PIXMAP_ALERTS, &mdf);  // draw filled
          }
          if (temp)
          {
            temp->flags[on_screen] = 'Y';
          }
        }
        else
        {
          // Not in our viewport, don't draw it!
          if (debug_level & 16)
          {
            fprintf(stderr,"load_alert_maps() Alert not visible\n");
          }
//fprintf(stderr,"B:%f  T:%f  L:%f  R:%f\n", temp->bottom_boundary, temp->top_boundary, temp->left_boundary, temp->right_boundary);
          if (temp)
          {
            temp->flags[on_screen] = 'N';
          }
        }
      }
      else
      {
        // Can't find this shape in the shapefile.
        if (debug_level & 16)
        {
          fprintf(stderr,
                  "load_alert_maps() Shape %s, strlen=%d, not found in %s\n",
                  temp->title,
                  (int)strlen(temp->title),
                  temp->filename );
        }
      }
    }
    temp = get_next_wx_alert(iterator);
  }
#ifndef USING_LIBGC
//fprintf(stderr,"free iterator 7\n");
  if (iterator)
  {
    free(iterator);
  }
#endif  // USING_LIBGC

  if (debug_level & 16)
  {
    fprintf(stderr,"load_alert_maps() Done drawing all active alerts\n");
  }

  if (alert_display_request())
  {
    alert_redraw_on_update = redraw_on_new_data = 2;
  }
}





// Here's the head of our sorted-by-layer maps list
static map_index_record *map_sorted_list_head = NULL;


static void empty_map_sorted_list(void)
{
  map_index_record *current = map_sorted_list_head;

  while (map_sorted_list_head != NULL)
  {
    current = map_sorted_list_head;
    map_sorted_list_head = current->next;
    if (current->XmStringPtr != NULL)
    {
      XmStringFree(current->XmStringPtr);
    }
    free(current);
  }
}





// Insert a map into the list at the end of the maps with the same
// layer number.  We'll need to look up the parameters for it from
// the master map_index list and then attach a new record to our new
// sorted list in the proper place.
//
// This function should be called when we're first starting up
// Xastir and anytime that selected_maps.sys is changed.
//
static void insert_map_sorted(char *filename)
{
  map_index_record *current;
  map_index_record *last;
  map_index_record *temp_record;
  unsigned long bottom;
  unsigned long top;
  unsigned long left;
  unsigned long right;
  int max_zoom;
  int min_zoom;
  int map_layer;
  int draw_filled;
  int usgs_drg;
  int auto_maps;
  int done;


  if (index_retrieve(filename,
                     &bottom,
                     &top,
                     &left,
                     &right,
                     &max_zoom,
                     &min_zoom,
                     &map_layer,
                     &draw_filled,
                     &usgs_drg,
                     &auto_maps))      // Found a match
  {

    // Allocate a new record
    temp_record = (map_index_record *)malloc(sizeof(map_index_record));
    CHECKMALLOC(temp_record);

    // Fill in the values
    xastir_snprintf(temp_record->filename,MAX_FILENAME,"%s",filename);
    temp_record->bottom = bottom;
    temp_record->top = top;
    temp_record->left = left;
    temp_record->right = right;
    temp_record->max_zoom = max_zoom;
    temp_record->min_zoom = min_zoom;
    temp_record->map_layer = map_layer;
    temp_record->draw_filled = draw_filled;
    temp_record->usgs_drg = usgs_drg;
    temp_record->auto_maps = auto_maps;
    temp_record->selected = 1;  // Always, we already know this!
    temp_record->accessed = 0;
    temp_record->next = NULL;
    temp_record->XmStringPtr = NULL;

    // Now find the proper place for it and insert it in
    // layer-order into the list.
    current = map_sorted_list_head;
    last = map_sorted_list_head;
    done = 0;

    // Possible cases:
    // Empty list
    // insert at beginning of list
    // insert at end of list
    // insert between other entries

    if (map_sorted_list_head == NULL)
    {
      // Empty list.  Insert record.
      map_sorted_list_head = temp_record;
      done++;
    }
    else if (map_layer < current->map_layer)
    {
      // Insert at beginning of list
      temp_record->next = current;
      map_sorted_list_head = temp_record;
      done++;
    }
    else    // Need to insert between records or at end of list
    {
      while (!done && (current != NULL) )
      {
        if (map_layer >= current->map_layer)    // Not to our layer yet
        {
          last = current;
          current = current->next;    // May point to NULL now
        }
        else if (map_layer < current->map_layer)
        {
          temp_record->next = current;
          last->next = temp_record;
          done++;
        }
      }
    }
    // Handle running off the end of the list
    if (!done && (current == NULL) )
    {
      last->next = temp_record;
    }
  }
  else
  {
    // We failed to find it in the map index
  }
}





/**********************************************************
 * load_auto_maps()
 *
 * NEW:  Uses the in-memory map_index to scan through the
 * maps.
 *
 * OLD: Recurses through the map directories looking for
 * maps to load.
 **********************************************************/
void load_auto_maps (Widget w, char * UNUSED(dir) )
{
  map_index_record *current = map_index_head;
  map_draw_flags mdf;

  HandlePendingEvents(app_context);
  if (interrupt_drawing_now)
  {
    return;
  }

  // Skip the sorting of the maps if we don't need to do it
  if (re_sort_maps)
  {

    //fprintf(stderr,"*** Sorting the selected maps by layer...\n");

    // Empty the sorted list first.  We'll create a new one.
    empty_map_sorted_list();

    // Run through the entire map_index linked list
    while (current != NULL)
    {
      if (auto_maps_skip_raster
          && (   strstr(current->filename,".geo")
                 || strstr(current->filename,".GEO")
                 || strstr(current->filename,".Geo")
                 || strstr(current->filename,".tif")
                 || strstr(current->filename,".TIF")
                 || strstr(current->filename,".Tif")))
      {
        // Skip this map
      }
      else    // Draw this map
      {

        //fprintf(stderr,"Loading: %s/%s\n",SELECTED_MAP_DIR,current->filename);

        //WE7U
        insert_map_sorted(current->filename);

        /*
                        draw_map (w,
                            SELECTED_MAP_DIR,
                            current->filename,
                            NULL,
                            '\0',
                            DRAW_TO_PIXMAP);
        */
      }
      current = current->next;
    }

    // All done sorting until something is changed in the Map
    // Chooser.
    re_sort_maps = 0;

    //fprintf(stderr,"*** DONE sorting the selected maps.\n");
  }

  // We have the maps in sorted order.  Run through the list and
  // draw them.  Only include those that have the auto_maps field
  // set to 1.
  current = map_sorted_list_head;
  while  (current != NULL)
  {

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

    if (disable_all_maps)
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

    // Debug
//        fprintf(stderr,"Drawing level:%05d, file:%s\n",
//            current->map_layer,
//            current->filename);

    // Draw the maps in sorted-by-layer order
    if (current->auto_maps)
    {

      mdf.draw_filled = current->draw_filled;
      mdf.usgs_drg = current->usgs_drg;

      if (debug_level & 16)
      {
        fprintf(stderr,"load_auto_maps: Calling draw_map\n");
      }

      draw_map (w,
                SELECTED_MAP_DIR,
                current->filename,
                NULL,
                '\0',
                DRAW_TO_PIXMAP,
                &mdf);
    }

    current = current->next;
  }
}





/*******************************************************************
 * load_maps()
 *
 * Loads maps, draws grid, updates the display.
 *
 * We now create a linked list of maps in layer-order and use this
 * list to draw the maps.  This preserves the correct ordering in
 * all cases.  The layer to draw each map is specified in the
 * map_index.sys file (fifth parameter).  Eventually code will be
 * added to the Map Chooser in order to change the layer each map is
 * drawn at.
 *******************************************************************/
void load_maps (Widget w)
{
  FILE *f;
  char mapname[MAX_FILENAME];
  int i;
  char selected_dir[MAX_FILENAME];
  map_index_record *current;
  map_draw_flags mdf;
  char selected_map_path[MAX_VALUE];

  get_user_base_dir(SELECTED_MAP_DATA, selected_map_path, sizeof(selected_map_path));

//    int dummy;


  if (debug_level & 16)
  {
    fprintf(stderr,"Load maps start\n");
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

  // Skip the sorting of the maps if we don't need to do it
  if (re_sort_maps)
  {

    //fprintf(stderr,"*** Sorting the selected maps by layer...\n");

    // Empty the sorted list first.  We'll create a new one.
    empty_map_sorted_list();

    // Make sure the string is empty before we start
    selected_dir[0] = '\0';

    // Create empty file if it doesn't exist
    (void)filecreate( selected_map_path );

    f = fopen ( selected_map_path, "r" );
    if (f != NULL)
    {
      if (debug_level & 16)
      {
        fprintf(stderr,"Load maps Open map file\n");
      }

      while (!feof (f))
      {

        // Grab one line from the file
        if ( fgets( mapname, MAX_FILENAME-1, f ) != NULL )
        {

          // Forced termination (just in case)
          mapname[MAX_FILENAME-1] = '\0';

          // Get rid of the newline at the end
          for (i = strlen(mapname); i > 0; i--)
          {
            if (mapname[i] == '\n')
            {
              mapname[i] = '\0';
            }
          }

          if (debug_level & 16)
          {
            fprintf(stderr,"Found mapname: %s\n", mapname);
          }

          // Test for comment
          if (mapname[0] != '#')
          {


            // Check whether it's a directory that was
            // selected.  If so, save it in a special
            // variable and use that to match all the files
            // inside the directory.  Note that with the way
            // we have things ordered in the list, the
            // directories appear before their member files.
            if (mapname[strlen(mapname)-1] == '/')
            {
              int len;

              // Found a directory.  Save the name.
              xastir_snprintf(selected_dir,
                              sizeof(selected_dir),
                              "%s",
                              mapname);

              len = strlen(mapname);

//fprintf(stderr,"Selected %s directory\n",selected_dir);

              // Here we need to run through the map_index
              // list to find all maps that match the
              // currently selected directory.  Attempt to
              // load all of those maps as well.

//fprintf(stderr,"Load all maps under this directory: %s\n",selected_dir);

              // Point to the start of the map_index list
              current = map_index_head;

              while (current != NULL)
              {

                if (strncmp(current->filename,selected_dir,len) == 0)
                {

                  if (current->filename[strlen(current->filename)-1] != '/')
                  {

//fprintf(stderr,"Loading: %s\n",current->filename);

                    //WE7U
                    insert_map_sorted(current->filename);

                    /*
                                                            draw_map (w,
                                                                SELECTED_MAP_DIR,
                                                                current->filename,
                                                                NULL,
                                                                '\0',
                                                                DRAW_TO_PIXMAP);
                    */

                  }
                }
                current = current->next;
              }
            }
            // Else must be a regular map file
            else
            {
//fprintf(stderr,"%s\n",mapname);
//start_timer();

              //WE7U
              insert_map_sorted(mapname);

              /*
                                          draw_map (w,
                                              SELECTED_MAP_DIR,
                                              mapname,
                                              NULL,
                                              '\0',
                                              DRAW_TO_PIXMAP);
              */

//stop_timer();
//print_timer_results();

              if (debug_level & 16)
              {
                fprintf(stderr,"Load maps -%s\n", mapname);
              }

              XmUpdateDisplay (da);
            }
          }
        }
        else    // We've hit EOF
        {
          break;
        }

      }
      (void)fclose (f);
      statusline(" ",1);      // delete status line
    }
    else
    {
      fprintf(stderr,"Couldn't open file: %s\n", selected_map_path );
    }

    // All done sorting until something is changed in the Map
    // Chooser.
    re_sort_maps = 0;

    //fprintf(stderr,"*** DONE sorting the selected maps.\n");
  }


  // We have the maps in sorted order.  Run through the list and
  // draw them.
  current = map_sorted_list_head;
  while  (current != NULL)
  {

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
    {
      statusline(" ",1);      // delete status line
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

    if (disable_all_maps)
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

    // Debug
//        fprintf(stderr,"Drawing level:%05d, file:%s\n",
//            current->map_layer,
//            current->filename);

    // Draw the maps in sorted-by-layer order
    mdf.draw_filled = current->draw_filled;
    mdf.usgs_drg = current->usgs_drg;

    if (debug_level & 16)
    {
      fprintf(stderr,"load_maps: Calling draw_map\n");
    }

// Map profiling, set up for 800x600 window at "Map Profile Test
// Site" bookmark.
//
// Loading "rd011802.shp" 500 times takes
// 302->256->264->269->115->116 seconds.
//
// 100 times on PP200 takes 192->183 seconds.
//
//start_timer();
//fprintf(stderr,"Calling draw_map() 500 times...\n");
//for (dummy = 0; dummy < 500; dummy++) {
    draw_map (w,
              SELECTED_MAP_DIR,
              current->filename,
              NULL,
              '\0',
              DRAW_TO_PIXMAP,
              &mdf);
//}
//stop_timer(); print_timer_results();

    current = current->next;
  }

  if (debug_level & 16)
  {
    fprintf(stderr,"Load maps stop\n");
  }
}


