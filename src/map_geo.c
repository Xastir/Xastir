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
 *
 *
 * Please see separate copyright notice attached to the
 * SHPRingDir_2d() function in this file.
 *
 */
#include "config.h"
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
#include <strings.h>

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

#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }

#ifdef HAVE_IMAGEMAGICK
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else   // TIME_WITH_SYS_TIME
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else  // HAVE_SYS_TIME_H
#  include <time.h>
# endif // HAVE_SYS_TIME_H
#endif  // TIME_WITH_SYS_TIME
#undef RETSIGTYPE
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
#include <magick/api.h>
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
#endif // HAVE_IMAGEMAGICK

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

struct FtpFile {
  char *filename;
  FILE *stream;
};
#endif

extern int npoints;		/* tsk tsk tsk -- globals */
extern int mag;


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
			 int draw_filled) {
    char file[MAX_FILENAME+1];      // Complete path/name of image file
    FILE *f;                        // Filehandle of image file
    char line[MAX_FILENAME];        // One line from GEO file
    char fileimg[MAX_FILENAME+1];   // Ascii name of image file, read from GEO file
    XpmAttributes atb;              // Map attributes after map's read into an XImage
    tiepoint tp[2];                 // Calibration points for map, read in from .geo file
    int n_tp;                       // Temp counter for number of tiepoints read
    float temp_long, temp_lat;
    register long map_c_T, map_c_L; // map delta NW edge coordinates, DNN: these should be signed
    register long tp_c_dx, tp_c_dy; // tiepoint coordinate differences
// DK7IN--
    int test;                       // temporary debugging

    unsigned long c_x_min,  c_y_min;// top left coordinates of map inside screen
    unsigned long c_y_max;          // bottom right coordinates of map inside screen
    double c_x;                     // Xastir coordinates 1/100 sec, 0 = 180°W
    double c_y;                     // Xastir coordinates 1/100 sec, 0 =  90°N
    double c_y_a;                   // coordinates correction for Transverse Mercator

    long map_y_0;                   // map pixel pointer prior to TM adjustment
    register long map_x, map_y;     // map pixel pointers, DNN: this was a float, chg to long
    long map_x_min, map_x_max;      // map boundaries for in screen part of map
    long map_y_min, map_y_max;      //
    long map_x_ctr;                 // half map width in pixel
    long map_y_ctr;                 // half map height in pixel
//    long x;
    int map_seen, map_act, map_done;
    double corrfact;

    long map_c_yc;                  // map center, vert coordinate
    long map_c_xc;                  // map center, hor  coordinate
    double map_c_dx, map_c_dy;      // map coordinates increment (pixel width)
    double c_dx;                    // adjusted map pixel width

    long scr_x,  scr_y;             // screen pixel plot positions
    long scr_xp, scr_yp;            // previous screen plot positions
    int  scr_dx, scr_dy;            // increments in screen plot positions
    long scr_x_mc;                  // map center in screen units

    long scr_c_xr;

    double dist;                    // distance from equator in nm
    double ew_ofs;                  // distance from map center in nm

    long scale_xa;                  // adjusted for topo maps
    double scale_x_nm;              // nm per Xastir coordinate unit
    long scale_x0;                  // at widest map area

#ifdef HAVE_IMAGEMAGICK
    char local_filename[MAX_FILENAME];
    ExceptionInfo exception;
    Image *image;
    ImageInfo *image_info;
    PixelPacket *pixel_pack;
    PixelPacket temp_pack;
    IndexPacket *index_pack;
    int l;
    XColor my_colors[256];
#ifdef HAVE_LIBCURL
    CURL *curl;
    CURLcode res;
    char curlerr[CURL_ERROR_SIZE];
    struct FtpFile ftpfile;
#else
#ifdef HAVE_WGET
    char tempfile[MAX_FILENAME];
#endif  // HAVE_WGET
#endif
    char gamma[16];
    struct {
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
    double left, right, top, bottom, map_width, map_height;
    //N0VH
//    double lat_center  = 0;
//    double long_center = 0;
    // Terraserver variables
    double top_n=0, left_e=0, bottom_n=0, right_e=0, map_top_n=0, map_left_e=0;
    int z, url_n=0, url_e=0, t_zoom=16, t_scale=12800;
    char zstr[8];
#else   // HAVE_IMAGEMAGICK
    XImage *xi;                 // Temp XImage used for reading in current image
#endif // HAVE_IMAGEMAGICK

    int terraserver_flag = 0;
    int toposerver_flag = 0;
    char map_it[MAX_FILENAME];
    int geo_image_width = 0;    // Image width  from GEO file
    int geo_image_height = 0;   // Image height from GEO file
    char geo_datum[8+1];        // WGS-84 etc.
    char geo_projection[8+1];   // TM, UTM, GK, LATLON etc.
    int map_proj;
    int map_refresh_interval_temp;
#ifdef FUZZYRASTER
    int rasterfuzz = 3;    // ratio to skip 
#endif //FUZZYRASTER
 
//#define TIMING_DEBUG
#ifdef TIMING_DEBUG
    time_mark(1);
#endif  // TIMING_DEBUG


    xastir_snprintf(file, sizeof(file), "%s/%s", dir, filenm);

    // Read the .geo file to find out map filename and tiepoint info
    n_tp = 0;
    geo_datum[0]      = '\0';
    geo_projection[0] = '\0';
    f = fopen (file, "r");
    if (f != NULL) {
        while (!feof (f)) {
            (void)get_line (f, line, MAX_FILENAME);
            if (strncasecmp (line, "FILENAME", 8) == 0) {
                (void)sscanf (line + 9, "%s", fileimg);
                if (fileimg[0] != '/' ) { // not absolute path
                    // make it relative to the .geo file
                    char temp[MAX_FILENAME];
                    strncpy(temp, file, MAX_FILENAME); // grab .geo file name
                    temp[MAX_FILENAME-1] = '\0';
                    (void)get_map_dir(temp);           // leaves just the path and trailing /
                    if (strlen(temp) < (MAX_FILENAME - 1 - strlen(fileimg)))
                        strcat(temp, fileimg);
                    strcpy(fileimg, temp);
                }
            }
            if (strncasecmp (line, "URL", 3) == 0)
                (void)sscanf (line + 4, "%s", fileimg);

            if (n_tp < 2) {     // Only take the first two tiepoints
                if (strncasecmp (line, "TIEPOINT", 8) == 0) {
                    (void)sscanf (line + 9, "%d %d %f %f",&tp[n_tp].img_x,&tp[n_tp].img_y,&temp_long,&temp_lat);
                    // Convert tiepoints from lat/lon to Xastir coordinates
                    tp[n_tp].x_long = 64800000l + (360000.0 * temp_long);
                    tp[n_tp].y_lat  = 32400000l + (360000.0 * (-temp_lat));
                    n_tp++;
                }
            }

            if (strncasecmp (line, "IMAGESIZE", 9) == 0)
                (void)sscanf (line + 10, "%d %d",&geo_image_width,&geo_image_height);

            if (strncasecmp (line, "DATUM", 5) == 0)
                (void)sscanf (line + 6, "%8s",geo_datum);

            if (strncasecmp (line, "PROJECTION", 10) == 0)
                (void)sscanf (line + 11, "%8s",geo_projection); // ignores leading and trailing space (nice!)

            if (strncasecmp (line, "TERRASERVER", 11) == 0)
                terraserver_flag = 1;

            if (strncasecmp (line, "TOPOSERVER", 10) == 0)
                toposerver_flag = 1;

            if (strncasecmp (line, "REFRESH", 7) == 0) {
                (void)sscanf (line + 8, "%d", &map_refresh_interval_temp);
                if ( map_refresh_interval_temp > 0 && 
                     ( map_refresh_interval == 0 || 
                       map_refresh_interval_temp < map_refresh_interval) ) {
                  map_refresh_interval = (time_t) map_refresh_interval_temp;
                  map_refresh_time = sec_now() + map_refresh_interval;
                  fprintf(stderr, "Map Refresh set to %d.\n", (int) map_refresh_interval);
                }
            }

#ifdef HAVE_IMAGEMAGICK
            if (strncasecmp(line, "GAMMA", 5) == 0)
                imagemagick_options.gamma_flag = sscanf(line + 6, "%f,%f,%f",
                                                        &imagemagick_options.r_gamma,
                                                        &imagemagick_options.g_gamma,
                                                        &imagemagick_options.b_gamma);
            if (strncasecmp(line, "CONTRAST", 8) == 0)
                (void)sscanf(line + 9, "%d", &imagemagick_options.contrast);
            if (strncasecmp(line, "NEGATE", 6) == 0)
                (void)sscanf(line + 7, "%d", &imagemagick_options.negate);
            if (strncasecmp(line, "EQUALIZE", 8) == 0)
                imagemagick_options.equalize = 1;
            if (strncasecmp(line, "NORMALIZE", 9) == 0)
                imagemagick_options.normalize = 1;
#if (MagickLibVersion >= 0x0539)
            if (strncasecmp(line, "LEVEL", 5) == 0) {
                strncpy(imagemagick_options.level, line + 6, 31);
                imagemagick_options.level[31] = '\0';
            }
#endif  // MagickLibVersion >= 0x0539
            if (strncasecmp(line, "MODULATE", 8) == 0) {
                strncpy(imagemagick_options.modulate, line + 9, 31);
                imagemagick_options.modulate[31] = '\0';
            }
#endif  // HAVE_IMAGEMAGICK
        }
        (void)fclose (f);
    }
    else {
        fprintf(stderr,"Couldn't open file: %s\n", file);
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
        strcpy(geo_projection,"LatLon");        // default
    //fprintf(stderr,"Map Projection: %s\n",geo_projection);
    (void)to_upper(geo_projection);
    if (strcmp(geo_projection,"TM") == 0)
        map_proj = 1;           // Transverse Mercator
    else
        map_proj = 0;           // Lat/Lon, default


#ifdef HAVE_IMAGEMAGICK
    if (terraserver_flag || toposerver_flag) {
//http://terraservice.net/download.ashx?t=1&s=10&x=2742&y=26372&z=10&w=820&h=480
        if (scale_y <= 4) {
                t_zoom  = 10; // 1m
                t_scale = 200;
        }
        else if (scale_y <= 8) {
             t_zoom  = 11; // 2m
             t_scale = 400;
        }
        else if (scale_y <= 16) {
            t_zoom  = 12; // 4m
            t_scale = 800;
        }
        else if (scale_y <= 32) {
            t_zoom  = 13; // 8m
            t_scale = 1600;
        }
        else if (scale_y <= 64) {
            t_zoom  = 14; // 16m
            t_scale = 3200;
        }
        else if (scale_y <= 128) {
            t_zoom  = 15; // 32m
            t_scale = 6400;
        }
        else {
            t_zoom  = 16; // 64m
            t_scale = 12800;
        }

        top  = -((y_lat_offset - 32400000l) / 360000.0);
        left =  (x_long_offset - 64800000l) / 360000.0;
        ll_to_utm(gDatum[D_NAD_83_CONUS].ellipsoid, top, left, &top_n, &left_e, zstr, sizeof(zstr) );
        sscanf(zstr, "%d", &z);

        bottom = -(((y_lat_offset + (screen_height * scale_y)) - 32400000l) / 360000.0);
        right  =   ((x_long_offset + (screen_width * scale_x)) - 64800000l) / 360000.0;
        ll_to_utm(gDatum[D_NAD_83_CONUS].ellipsoid, bottom, right, &bottom_n, &right_e, zstr, sizeof(zstr) );

        map_top_n  = (int)((top_n  / t_scale) + 1) * t_scale;
        map_left_e = (int)((left_e / t_scale) + 0) * t_scale;
        utm_to_ll(gDatum[D_NAD_83_CONUS].ellipsoid, map_top_n, map_left_e, zstr, &top, &left);

        geo_image_height = (map_top_n - bottom_n) * 200 / t_scale;
        geo_image_width  = (right_e - map_left_e) * 200 / t_scale;
        map_width  = right - left;
        map_height = top - bottom;

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
//          "http://terraservice.net/download.ashx?t=%d\046s=%d\046x=%d\046y=%d\046z=%d\046w=%d\046h=%d",
            "http://terraserver-usa.net/download.ashx?t=%d\046s=%d\046x=%d\046y=%d\046z=%d\046w=%d\046h=%d",
            (toposerver_flag) ? 2 : 1,
            t_zoom,
            url_e,
            url_n,
            z,
            geo_image_width,
            geo_image_height);
    }
#endif // HAVE_IMAGEMAGICK

    //
    // DK7IN: we should check what we got from the geo file
    //   we use geo_image_width, but it might not be initialised...
    //   and it's wrong if the '\n' is missing a the end...

    /*
    * Here are the corners of our viewport, using the Xastir
    * coordinate system.  Notice that Y is upside down:
    *
    *   left edge of view = x_long_offset
    *  right edge of view = x_long_offset + (screen_width  * scale_x)
    *    top edge of view =  y_lat_offset
    * bottom edge of view =  y_lat_offset + (screen_height * scale_y)
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
    map_c_L = tp[0].x_long - x_long_offset;     // map left coordinate
    map_c_T = tp[0].y_lat  - y_lat_offset;      // map top  coordinate

    tp_c_dx = (long)(tp[1].x_long - tp[0].x_long);//  Width between tiepoints
    tp_c_dy = (long)(tp[1].y_lat  - tp[0].y_lat); // Height between tiepoints


    // Check for tiepoints being in wrong relation to one another
    if (tp_c_dx < 0) tp_c_dx = -tp_c_dx;       // New  width between tiepoints
    if (tp_c_dy < 0) tp_c_dy = -tp_c_dy;       // New height between tiepoints


    if (debug_level & 512) {
        fprintf(stderr,"X tiepoint width: %ld\n", tp_c_dx);
        fprintf(stderr,"Y tiepoint width: %ld\n", tp_c_dy);
    }

    // Calculate step size per pixel
    map_c_dx = ((double) tp_c_dx / abs(tp[1].img_x - tp[0].img_x));
    map_c_dy = ((double) tp_c_dy / abs(tp[1].img_y - tp[0].img_y));

    // Scaled screen step size for use with XFillRectangle below
    scr_dx = (int) (map_c_dx / scale_x) + 1;
    scr_dy = (int) (map_c_dy / scale_y) + 1;

    if (debug_level & 512) {
        fprintf(stderr,"\nImage: %s\n", file);
        fprintf(stderr,"Image size %d %d\n", geo_image_width, geo_image_height);
        fprintf(stderr,"XX: %ld YY:%ld Sx %f %d Sy %f %d\n", map_c_L, map_c_T, map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));
    }

    // calculate top left map corner from tiepoints
    if (tp[0].img_x != 0) {
        tp[0].x_long -= (tp[0].img_x * map_c_dx);   // map left edge longitude
        map_c_L = tp[0].x_long - x_long_offset;     // delta ??
        tp[0].img_x = 0;
        if (debug_level & 512)
            fprintf(stderr,"Translated tiepoint_0 x: %d\t%lu\n", tp[0].img_x, tp[0].x_long);
    }
    if (tp[0].img_y != 0) {
        tp[0].y_lat -= (tp[0].img_y * map_c_dy);    // map top edge latitude
        map_c_T = tp[0].y_lat - y_lat_offset;
        tp[0].img_y = 0;
        if (debug_level & 512)
            fprintf(stderr,"Translated tiepoint_0 y: %d\t%lu\n", tp[0].img_y, tp[0].y_lat);
    }

    // By this point, geo_image_width & geo_image_height have to
    // have been initialized to something.

    if ( (geo_image_width == 0) || (geo_image_height == 0) ) {

        if ( (strncasecmp ("http", fileimg, 4) == 0)
             || (strncasecmp ("ftp", fileimg, 3) == 0)) {
            // what to do for remote files... hmm... -cbell
        } else {

#ifdef HAVE_IMAGEMAGICK
            GetExceptionInfo(&exception);
            image_info=CloneImageInfo((ImageInfo *) NULL);
            (void) strcpy(image_info->filename, fileimg);
            if (debug_level & 16) {
                fprintf(stderr,"Copied %s into image info.\n", file);
                fprintf(stderr,"image_info got: %s\n", image_info->filename);
                fprintf(stderr,"Entered ImageMagick code.\n");
                fprintf(stderr,"Attempting to open: %s\n", image_info->filename);
            }
            
            // We do a test read first to see if the file exists, so we
            // don't kill Xastir in the ReadImage routine.
            f = fopen (image_info->filename, "r");
            if (f == NULL) {
                fprintf(stderr,"File %s could not be read\n",image_info->filename);
                return;
            }
            (void)fclose (f);
            
            image = PingImage(image_info, &exception);
        
            if (image == (Image *) NULL) {
                MagickWarning(exception.severity, exception.reason, exception.description);
                //fprintf(stderr,"MagickWarning\n");
                return;
            }
            
            if (debug_level & 16)
                fprintf(stderr,"Color depth is %i \n", (int)image->depth);
            
            geo_image_width = image->magick_columns;
            geo_image_height = image->magick_rows;
     
            // close and clean up imagemagick
        
            if (image)
                DestroyImage(image);
            if (image_info)
                DestroyImageInfo(image_info);
#endif // HAVE_IMAGEMAGICK
        }
    }

    //    fprintf(stderr, "Geo: %s: size %ux%u.\n",file, geo_image_width, geo_image_height);
    // if that did not generate a valid size, bail out... 
    if ( (geo_image_width == 0) || (geo_image_height == 0) ) {
        fprintf(stderr,"*** Skipping '%s', IMAGESIZE tag missing or incorrect. ***\n",file);
        fprintf(stderr,"Perhaps no XPM or ImageMagick library support is installed?\n");
        return;
    }
    // calculate bottom right map corner from tiepoints
    // map size is geo_image_width / geo_image_height
    if (tp[1].img_x != (geo_image_width - 1) ) {
        tp[1].img_x = geo_image_width - 1;
        tp[1].x_long = tp[0].x_long + (tp[1].img_x * map_c_dx); // right
        if (debug_level & 512)
            fprintf(stderr,"Translated tiepoint_1 x: %d\t%lu\n", tp[1].img_x, tp[1].x_long);
    }
    if (tp[1].img_y != (geo_image_height - 1) ) {
        tp[1].img_y = geo_image_height - 1;
        tp[1].y_lat = tp[0].y_lat + (tp[1].img_y * map_c_dy);   // bottom
        if (debug_level & 512)
            fprintf(stderr,"Translated tiepoint_1 y: %d\t%lu\n", tp[1].img_y, tp[1].y_lat);
    }


    // Check whether we're indexing or drawing the map
    if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
            || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {

        // We're indexing only.  Save the extents in the index.
        if (terraserver_flag || toposerver_flag) {
            // Force the extents to the edges of the earth for the
            // index file.
            index_update_xastir(filenm, // Filename only
                64800000l,      // Bottom
                0l,             // Top
                0l,             // Left
                129600000l);    // Right
        }
        else {
            index_update_xastir(filenm, // Filename only
                tp[1].y_lat,    // Bottom
                tp[0].y_lat,    // Top
                tp[0].x_long,   // Left
                tp[1].x_long);  // Right
        }

        // Update statusline
        xastir_snprintf(map_it, sizeof(map_it), langcode ("BBARSTA039"), filenm);
        statusline(map_it,0);       // Loading/Indexing ...

        return; // Done indexing this file
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return;

    // Check whether map is inside our current view
    //                bottom        top    left        right
    if (!map_visible (tp[1].y_lat, tp[0].y_lat, tp[0].x_long, tp[1].x_long)) {
        if (debug_level & 16) {
            fprintf(stderr,"Map not in current view, skipping: %s\n", file);
            fprintf(stderr,"\nImage: %s\n", file);
            fprintf(stderr,"Image size %d %d\n", geo_image_width, geo_image_height);
            fprintf(stderr,"XX: %ld YY:%ld Sx %f %d Sy %f %d\n", map_c_L, map_c_T, map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));
        }
        return;            /* Skip this map */
#ifdef FUZZYRASTER 
   } else if (((float)(map_c_dx/scale_x) > rasterfuzz) ||
               ((float)(scale_x/map_c_dx) > rasterfuzz) ||
               ((float)(map_c_dy/scale_y) > rasterfuzz) ||
               ((float)(scale_y/map_c_dy) > rasterfuzz)) {
      printf("Skipping fuzzy map %s with sx=%f,sy=%f.\n", file,
             (float)(map_c_dx/scale_x),(float)(map_c_dy/scale_y));
      return;
#endif //FUZZYRASTER
    } else if (debug_level & 16) {
        fprintf(stderr,"Loading imagemap: %s\n", file);
        fprintf(stderr,"\nImage: %s\n", file);
        fprintf(stderr,"Image size %d %d\n", geo_image_width, geo_image_height);
        fprintf(stderr,"XX: %ld YY:%ld Sx %f %d Sy %f %d\n", map_c_L, map_c_T, map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));
    }

    // Update statusline
    xastir_snprintf(map_it, sizeof(map_it), langcode ("BBARSTA028"), filenm);
    statusline(map_it,0);       // Loading/Indexing ...

    atb.valuemask = 0;

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return;

// Best here would be to add the process ID or user ID to the filename
// (to keep the filename distinct for different users), and to check
// the timestamp on the map file.  If it's older than xx minutes, go
// get another one.  Make sure to delete the temp files when closing
// Xastir.  It'd probably be good to check for old files and delete
// them when starting Xastir as well.

    // Check to see if we have to use "wget" to go get an internet map
    if ( (strncasecmp ("http", fileimg, 4) == 0)
            || (strncasecmp ("ftp", fileimg, 3) == 0)
            || (terraserver_flag)
            || (toposerver_flag) ) {
#ifdef HAVE_IMAGEMAGICK
        char *ext;

        if (debug_level & 16)
            fprintf(stderr,"ftp or http file: %s\n", fileimg);

        if (terraserver_flag || toposerver_flag)
            ext = "jpg";
        else
            ext = get_map_ext(fileimg); // Use extension to determine image type

        xastir_snprintf(local_filename,
            sizeof(local_filename),
            "%s/map.%s",
            get_user_base_dir("tmp"),ext);

#ifdef HAVE_LIBCURL
        curl = curl_easy_init();

        if (curl) { 

            /* verbose debug is keen */
          //            curl_easy_setopt(curl, CURLOPT_VERBOSE, TRUE);
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlerr);

            /* write function */
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_fwrite);

            /* download from fileimg */
            curl_easy_setopt(curl, CURLOPT_URL, fileimg);

            /* save as local_filename */
            ftpfile.filename = local_filename;
            ftpfile.stream = NULL;
            curl_easy_setopt(curl, CURLOPT_FILE, &ftpfile);    

            res = curl_easy_perform(curl);

            curl_easy_cleanup(curl);

            if (CURLE_OK != res) {
                fprintf(stderr, "curl told us %d\n", res);
                fprintf(stderr, "curlerr is %s\n", curlerr);
            }

            if (ftpfile.stream)
                fclose(ftpfile.stream);

            // Return if we had trouble
            if (CURLE_OK != res) {
                return;
            }

        } else { 
            fprintf(stderr,"Couldn't download the geo or Terraserver image\n");
            return;
        }


#else
#ifdef HAVE_WGET
        xastir_snprintf(tempfile, sizeof(tempfile),
                "%s --server-response --timestamping --tries=1 --timeout=30 --output-document=%s \'%s\' 2> /dev/null\n",
                WGET_PATH,
                local_filename,
                fileimg);

        if (debug_level & 16)
            fprintf(stderr,"%s",tempfile);

//fprintf(stderr,"Getting file\n");
        if ( system(tempfile) ) {   // Go get the file
            fprintf(stderr,"Couldn't download the geo or Terraserver image\n");
            return;
        }
#else   // HAVE_WGET
        fprintf(stderr,"libcurl or 'wget' not installed.  Can't download image\n");
#endif  // HAVE_WGET
#endif  // HAVE_LIBCURL

        // Set permissions on the file so that any user can overwrite it.
        chmod(local_filename, 0666);

        // We now re-use the "file" variable.  It'll hold the
        //name of the map file now instead of the .geo file.
        strcpy(file,local_filename);  // Tell ImageMagick where to find it
#endif  // HAVE_IMAGEMAGICK

    } else {
        //fprintf(stderr,"Not ftp or http file\n");

        // We now re-use the "file" variable.  It'll hold the
        //name of the map file now instead of the .geo file.
        strcpy (file, fileimg);
    }

    //fprintf(stderr,"File = %s\n",file);

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return;

// The status line is not updated yet, probably 'cuz we're too busy
// getting the map in this thread and aren't redrawing?


#ifdef HAVE_IMAGEMAGICK
    GetExceptionInfo(&exception);
    image_info=CloneImageInfo((ImageInfo *) NULL);
    (void) strcpy(image_info->filename, file);
    if (debug_level & 16) {
           fprintf(stderr,"Copied %s into image info.\n", file);
           fprintf(stderr,"image_info got: %s\n", image_info->filename);
           fprintf(stderr,"Entered ImageMagick code.\n");
           fprintf(stderr,"Attempting to open: %s\n", image_info->filename);
    }

    // We do a test read first to see if the file exists, so we
    // don't kill Xastir in the ReadImage routine.
    f = fopen (image_info->filename, "r");
    if (f == NULL) {
        fprintf(stderr,"File %s could not be read\n",image_info->filename);
        return;
    }
    (void)fclose (f);

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return;

    image = ReadImage(image_info, &exception);

    if (image == (Image *) NULL) {
        MagickWarning(exception.severity, exception.reason, exception.description);
        //fprintf(stderr,"MagickWarning\n");
        return;
    }

    if (debug_level & 16)
        fprintf(stderr,"Color depth is %i \n", (int)image->depth);

    if (image->colorspace != RGBColorspace) {
        fprintf(stderr,"TBD: I don't think we can deal with colorspace != RGB");
        if (image)
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    atb.width = image->columns;
    atb.height = image->rows;

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        if (image)
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    // gamma setup
    if (imagemagick_options.gamma_flag == 0 ||
        imagemagick_options.gamma_flag == 1) {
        if (imagemagick_options.gamma_flag == 0) // if not set in file, set to 1.0
            imagemagick_options.r_gamma = 1.0;

        imagemagick_options.gamma_flag = 1; // set flag to do gamma

        imagemagick_options.r_gamma += imagemagick_gamma_adjust;

        if (imagemagick_options.r_gamma > 0.95 && imagemagick_options.r_gamma < 1.05)
            imagemagick_options.gamma_flag = 0; // don't bother if near 1.0
        else if (imagemagick_options.r_gamma < 0.1)
            imagemagick_options.r_gamma = 0.1; // 0.0 is black and negative is really wacky

        xastir_snprintf(gamma, sizeof(gamma), "%.1f", imagemagick_options.r_gamma);
    }
    else if (imagemagick_options.gamma_flag == 3) {
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
        imagemagick_options.gamma_flag = 0;

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        if (image) 
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    if (imagemagick_options.gamma_flag) {
        if (debug_level & 16)
            fprintf(stderr,"gamma=%s\n", gamma);
        GammaImage(image, gamma);
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        if (image) 
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    if (imagemagick_options.contrast != 0) {
        if (debug_level & 16)
            fprintf(stderr,"contrast=%d\n", imagemagick_options.contrast);
        ContrastImage(image, imagemagick_options.contrast);
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        if (image) 
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    if (imagemagick_options.negate != -1) {
        if (debug_level & 16)
            fprintf(stderr,"negate=%d\n", imagemagick_options.negate);
        NegateImage(image, imagemagick_options.negate);
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        if (image) 
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    if (imagemagick_options.equalize) {
        if (debug_level & 16)
            fprintf(stderr,"equalize");
        EqualizeImage(image);
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        if (image) 
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    if (imagemagick_options.normalize) {
        if (debug_level & 16)
            fprintf(stderr,"normalize");
        NormalizeImage(image);
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        if (image) 
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

#if (MagickLibVersion >= 0x0539)
    if (imagemagick_options.level[0] != '\0') {
        if (debug_level & 16)
            fprintf(stderr,"level=%s\n", imagemagick_options.level);
        LevelImage(image, imagemagick_options.level);
    }
#endif  // MagickLibVersion >= 0x0539

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        if (image) 
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    if (imagemagick_options.modulate[0] != '\0') {
        if (debug_level & 16)
            fprintf(stderr,"modulate=%s\n", imagemagick_options.modulate);
        ModulateImage(image, imagemagick_options.modulate);
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        if (image) 
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    // If were are drawing to a low bpp display (typically < 8bpp)
    // try to reduce the number of colors in an image.
    // This may take some time, so it would be best to do ahead of
    // time if it is a static image.
#if (MagickLibVersion < 0x0540)
    if (visual_type == NOT_TRUE_NOR_DIRECT && GetNumberColors(image, NULL) > 128) {
#else   // MagickLib >= 540
    if (visual_type == NOT_TRUE_NOR_DIRECT && GetNumberColors(image, NULL, &exception) > 128) {
#endif  // MagickLib Version

        if (image->storage_class == PseudoClass) {
#if (MagickLibVersion < 0x0549)
            CompressColormap(image); // Remove duplicate colors
#else // MagickLib >= 0x0549
            CompressImageColormap(image); // Remove duplicate colors
#endif  // MagickLibVersion < 0x0549
        }

        // Quantize down to 128 will go here...
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        if (image) 
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    pixel_pack = GetImagePixels(image, 0, 0, image->columns, image->rows);
    if (!pixel_pack) {
        fprintf(stderr,"pixel_pack == NULL!!!");
        if (image)
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        if (image) 
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    index_pack = GetIndexes(image);
    if (image->storage_class == PseudoClass && !index_pack) {
        fprintf(stderr,"PseudoClass && index_pack == NULL!!!");
        if (image)
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        if (image) 
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    if (image->storage_class == PseudoClass && image->colors <= 256) {
        for (l = 0; l < (int)image->colors; l++) {
            // Need to check how to do this for ANY image, as ImageMagick can read in all sorts
            // of image files
            temp_pack = image->colormap[l];
            if (debug_level & 16)
                fprintf(stderr,"Colormap color is %i  %i  %i \n",
                       temp_pack.red, temp_pack.green, temp_pack.blue);

            // Here's a tricky bit:  PixelPacket entries are defined as Quantum's.  Quantum
            // is defined in /usr/include/magick/image.h as either an unsigned short or an
            // unsigned char, depending on what "configure" decided when ImageMagick was installed.
            // We can determine which by looking at MaxRGB or QuantumDepth.
            //
            if (QuantumDepth == 16) {   // Defined in /usr/include/magick/image.h
                if (debug_level & 16)
                    fprintf(stderr,"Color quantum is [0..65535]\n");
                my_colors[l].red   = temp_pack.red;
                my_colors[l].green = temp_pack.green;
                my_colors[l].blue  = temp_pack.blue;
            }
            else {  // QuantumDepth = 8
                if (debug_level & 16)
                    fprintf(stderr,"Color quantum is [0..255]\n");
                my_colors[l].red   = temp_pack.red   << 8;
                my_colors[l].green = temp_pack.green << 8;
                my_colors[l].blue  = temp_pack.blue  << 8;
            }

            // Get the color allocated on < 8bpp displays. pixel color is written to my_colors.pixel
            if (visual_type == NOT_TRUE_NOR_DIRECT)
                XAllocColor(XtDisplay(w), cmap, &my_colors[l]);
            else
                pack_pixel_bits(my_colors[l].red, my_colors[l].green, my_colors[l].blue,
                                &my_colors[l].pixel);

            if (debug_level & 16)
                fprintf(stderr,"Color allocated is %li  %i  %i  %i \n", my_colors[l].pixel,
                       my_colors[l].red, my_colors[l].blue, my_colors[l].green);
        }
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        if (image) 
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

#ifdef TIMING_DEBUG
    time_mark(0);
#endif  // TIMING_DEBUG

    if (debug_level & 16) {
       fprintf(stderr,"Image size %d %d\n", atb.width, atb.height);
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
            fprintf(stderr,"Class Type = Undefined\n");
       else if (image->colorspace == RGBColorspace)
            fprintf(stderr,"Class Type = RGBColorspace\n");
       else if (image->colorspace == GRAYColorspace)
            fprintf(stderr,"Class Type = GRAYColorspace\n");
       else if (image->colorspace == sRGBColorspace)
            fprintf(stderr,"Class Type = sRGBColorspace\n");
    }

#else   // HAVE_IMAGEMAGICK

    // We don't have ImageMagick libs compiled in, so use the
    // XPM library instead.

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return;

    /*  XpmReadFileToImage is the call we wish to avoid if at all
     *  possible.  On large images this can take quite a while.  We
     *  check above to see whether the image is inside our viewport,
     *  and if not we skip loading the image.
     */
    if (! XpmReadFileToImage (XtDisplay (w), file, &xi, NULL, &atb) == XpmSuccess) {
        fprintf(stderr,"ERROR loading %s\n", file);
        if (xi)
            XDestroyImage (xi);
        return;
    }
    if (debug_level & 16) {
        fprintf(stderr,"Image size %d %d\n", (int)atb.width, (int)atb.height);
        fprintf(stderr,"XX: %ld YY:%ld Sx %f %d Sy %f %d\n", map_c_L, map_c_T, map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        if (xi)
            XDestroyImage (xi);
        return;
    }

#endif  // HAVE_IMAGEMAGICK

    // draw the image from the file out to the map screen

    // Get the border values for the X and Y for loops used
    // for the XFillRectangle call later.

    map_c_yc = (tp[0].y_lat + tp[1].y_lat) / 2;     // vert center of map as reference
    map_y_ctr = (long)(atb.height / 2 +0.499);
    scale_x0 = get_x_scale(0,map_c_yc,scale_y);     // reference scaling at vert map center

    map_c_xc  = (tp[0].x_long + tp[1].x_long) / 2;  // hor center of map as reference
    map_x_ctr = (long)(atb.width  / 2 +0.499);
    scr_x_mc  = (map_c_xc - x_long_offset) / scale_x; // screen coordinates of map center

    // calculate map pixel range in y direction that falls into screen area
    c_y_max = 0ul;
    map_y_min = map_y_max = 0l;
    for (map_y_0 = 0, c_y = tp[0].y_lat; map_y_0 < (long)atb.height; map_y_0++, c_y += map_c_dy) {
        scr_y = (c_y - y_lat_offset) / scale_y;   // current screen position
        if (scr_y > 0) {
            if (scr_y < screen_height) {
                map_y_max = map_y_0;          // update last map pixel in y
                c_y_max = (unsigned long)c_y;// bottom map inside screen coordinate
            } else
                break;                      // done, reached bottom screen border
        } else {                            // pixel is above screen
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
        for (map_x = 0, c_x = tp[0].x_long; map_x < (long)atb.width; map_x++, c_x += map_c_dx) {
            scr_x = (c_x - x_long_offset)/ scale_x;  // current screen position
            if (scr_x > 0) {
                if (scr_x < screen_width)
                    map_x_max = map_x;          // update last map pixel in x
                else
                    break;                      // done, reached right screen border
            } else {                            // pixel is left from screen
                map_x_min = map_x;              // update first map pixel in x
            }
        }
        c_x_min = (unsigned long)(tp[0].x_long + map_x_min * map_c_dx);   // left map inside screen coordinate
//    }
//    for (scr_y = scr_y_min; scr_y <= scr_y_max;scr_y++) {       // screen lines
//    }

    test = 1;           // DK7IN: debuging
    scr_yp = -1;
    scr_c_xr = x_long_offset + screen_width * scale_x;
    c_dx = map_c_dx;                            // map pixel width
    scale_xa = scale_x0;                        // the compiler likes it ;-)

//    for (map_y_0 = 0, c_y = tp[0].y_lat; map_y_0 < (long)atb.height; map_y_0++, c_y += map_c_dy) {
//        scr_y = (c_y - y_lat_offset) / scale_y;   // current screen position

    map_done = 0;
    map_act  = 0;
    map_seen = 0;
    scr_y = screen_height - 1;

#ifdef TIMING_DEBUG
    time_mark(0);
#endif  // TIMING_DEBUG


    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
#ifdef HAVE_IMAGEMAGICK
        if (image)
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
#else   // HAVE_IMAGEMAGICK
        if (xi)
            XDestroyImage (xi);
#endif  // HAVE_IMAGEMAGICK
        return;
    }


    // loop over map pixel rows
    for (map_y_0 = map_y_min, c_y = (double)c_y_min;
                (map_y_0 <= map_y_max) || (map_proj == 1 && !map_done && scr_y < screen_height);
                map_y_0++, c_y += map_c_dy) {

        scr_y = (c_y - y_lat_offset) / scale_y;
        if (scr_y != scr_yp) {                  // don't do a row twice
            scr_yp = scr_y;                     // remember as previous y
            if (map_proj == 1) {                // Transverse Mercator correction in x
                scale_xa = get_x_scale(0,(long)c_y,scale_y); // recalc scale_x for current y
                c_dx = map_c_dx * scale_xa / scale_x0;       // adjusted map pixel width

                map_x_min = map_x_ctr - (map_c_xc - x_long_offset) / c_dx;
                if (map_x_min < 0)
                    map_x_min = 0;
                c_x_min = map_c_xc - (map_x_ctr - map_x_min) * c_dx;
                map_x_max = map_x_ctr - (map_c_xc - scr_c_xr) / c_dx;
                if (map_x_max > (long)atb.width)
                    map_x_max = atb.width;
                scr_dx = (int) (c_dx / scale_x) + 1;    // at least 1 pixel wide
            }

//            if (c_y == (double)c_y_min) {  // first call
//                fprintf(stderr,"map: min %ld ctr %ld max %ld, c_dx %ld, c_x_min %ld, c_y_min %ld\n",map_x_min,map_x_ctr,map_x_max,(long)c_dx,c_x_min,c_y_min);
//            }
            scr_xp = -1;
            // loop over map pixel columns
            map_act = 0;
            scale_x_nm = calc_dscale_x(0,(long)c_y) / 1852.0;  // nm per Xastir coordinate
            for (map_x = map_x_min, c_x = (double)c_x_min; map_x <= map_x_max; map_x++, c_x += c_dx) {
                scr_x = (c_x - x_long_offset) / scale_x;
                if (scr_x != scr_xp) {      // don't do a pixel twice
                    scr_xp = scr_x;         // remember as previous x
                    if (map_proj == 1) {    // Transverse Mercator correction in y
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
                            map_y = map_y_0 + (long)(corrfact*c_y_a / map_c_dy);  // coord per pixel
                        else                    // N
                            map_y = map_y_0 - (long)(corrfact*c_y_a / map_c_dy);
//                        if (test < 10) {
//                            fprintf(stderr,"dist: %ldkm, ew_ofs: %ldkm, dy: %ldkm\n",(long)(1.852*dist),(long)(1.852*ew_ofs),(long)(1.852*c_y_a/6000.0));
//                            fprintf(stderr,"  corrfact: %f, mapy0: %ld, mapy: %ld\n",corrfact,map_y_0,map_y);
//                            test++;
//                        }
                    } else {
                        map_y = map_y_0;
                    }

                    if (map_y >= 0 && map_y <= tp[1].img_y) { // check map boundaries in y direction
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
#ifdef HAVE_IMAGEMAGICK
                        l = map_x + map_y * image->columns;
                        if (image->storage_class == PseudoClass) {
                            XSetForeground(XtDisplay(w), gc, my_colors[index_pack[l]].pixel);
                        }
                        else {
                            pack_pixel_bits(pixel_pack[l].red,
                                            pixel_pack[l].green,
                                            pixel_pack[l].blue,
                                            &my_colors[0].pixel);
                            XSetForeground(XtDisplay(w), gc, my_colors[0].pixel);
                        }
#else   // HAVE_IMAGEMAGICK
                        (void)XSetForeground (XtDisplay (w), gc, XGetPixel (xi, map_x, map_y));
#endif  // HAVE_IMAGEMAGICK
                        (void)XFillRectangle (XtDisplay (w),pixmap,gc,scr_x,scr_y,scr_dx,scr_dy);
                    } // check map boundaries in y direction
                }
            } // loop over map pixel columns
            if (map_seen && !map_act)
                map_done = 1;
        }
    } // loop over map pixel rows

#ifdef HAVE_IMAGEMAGICK
    if (image)
       DestroyImage(image);
    if (image_info)
       DestroyImageInfo(image_info);
#else   // HAVE_IMAGEMAGICK
    if (xi)
        XDestroyImage (xi);
#endif // HAVE_IMAGEMAGICK

#ifdef TIMING_DEBUG
    time_mark(0);
#endif  // TIMING_DEBUG
}


