/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
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
 * draw_tiger_map()
 * N0VH
 **********************************************************/
#ifdef HAVE_IMAGEMAGICK
void draw_tiger_map (Widget w) {
    char file[MAX_FILENAME];        // Complete path/name of image file
    FILE *f;                        // Filehandle of image file
    char fileimg[MAX_FILENAME];     // Ascii name of image file, read from GEO file
    char tigertmp[MAX_FILENAME*2];  // Used for putting together the tigermap query
    int width, height;
    tiepoint tp[2];                 // Calibration points for map, read in from .geo file
    register long map_c_T, map_c_L; // map delta NW edge coordinates, DNN: these should be signed
    register long tp_c_dx, tp_c_dy; // tiepoint coordinate differences
    unsigned long c_x_min,  c_y_min;// top left coordinates of map inside screen
    unsigned long c_y_max;          // bottom right coordinates of map inside screen
    double c_x;                     // Xastir coordinates 1/100 sec, 0 = 180°W
    double c_y;                     // Xastir coordinates 1/100 sec, 0 =  90°N

    long map_y_0;                   // map pixel pointer prior to TM adjustment
    register long map_x, map_y;     // map pixel pointers, DNN: this was a float, chg to long
    long map_x_min, map_x_max;      // map boundaries for in screen part of map
    long map_y_min, map_y_max;      //
    long map_x_ctr;                 // half map width in pixel
    long map_y_ctr;                 // half map height in pixel
    int map_seen, map_act, map_done;

    long map_c_yc;                  // map center, vert coordinate
    long map_c_xc;                  // map center, hor  coordinate
    double map_c_dx, map_c_dy;      // map coordinates increment (pixel width)
    double c_dx;                    // adjusted map pixel width

    long scr_x,  scr_y;             // screen pixel plot positions
    long scr_xp, scr_yp;            // previous screen plot positions
    int  scr_dx, scr_dy;            // increments in screen plot positions
    long scr_x_mc;                  // map center in screen units

    long scr_c_xr;

    long scale_xa;                  // adjusted for topo maps
    double scale_x_nm;              // nm per Xastir coordinate unit
    long scale_x0;                  // at widest map area

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
    double left, right, top, bottom, map_width, map_height;
    double lat_center  = 0;
    double long_center = 0;

    char map_it[MAX_FILENAME];
    char tmpstr[100];
    int geo_image_width;        // Image width  from GEO file
    int geo_image_height;       // Image height from GEO file


    xastir_snprintf(map_it, sizeof(map_it), langcode ("BBARSTA028"), "tigermap");
    statusline(map_it,0);       // Loading ...


    // Tiepoint for upper left screen corner
    //
    tp[0].img_x = 0;                // Pixel Coordinates
    tp[0].img_y = 0;                // Pixel Coordinates
    tp[0].x_long = x_long_offset;   // Xastir Coordinates
    tp[0].y_lat  = y_lat_offset;    // Xastir Coordinates

    // Tiepoint for lower right screen corner
    //
    tp[1].img_x =  screen_width - 1; // Pixel Coordinates
    tp[1].img_y = screen_height - 1; // Pixel Coordinates 

    tp[1].x_long = x_long_offset + (( screen_width) * scale_x); // Xastir Coordinates
    tp[1].y_lat  =  y_lat_offset + ((screen_height) * scale_y); // Xastir Coordinates

    left = (double)((x_long_offset - 64800000l )/360000.0);   // Lat/long Coordinates
    top = (double)(-((y_lat_offset - 32400000l )/360000.0));  // Lat/long Coordinates
    right = (double)(((x_long_offset + ((screen_width) * scale_x) ) - 64800000l)/360000.0);//Lat/long Coordinates
    bottom = (double)(-(((y_lat_offset + ((screen_height) * scale_y) ) - 32400000l)/360000.0));//Lat/long Coordinates

    map_width = right - left;   // Lat/long Coordinates
    map_height = top - bottom;  // Lat/long Coordinates

    geo_image_width  = screen_width;
    geo_image_height = screen_height;

    long_center = (left + right)/2.0l;
    lat_center  = (top + bottom)/2.0l;

//  Example query to the census map server....
/*        xastir_snprintf(fileimg, sizeof(fileimg), 
        "\'http://tiger.census.gov/cgi-bin/mapper/map.gif?on=CITIES&on=GRID&on=counties&on=majroads&on=places&&on=interstate&on=states&on=ushwy&on=statehwy&lat=%f\046lon=%f\046wid=%f\046ht=%f\046iwd=%i\046iht=%i\'",\
                   lat_center, long_center, map_width, map_height, tp[1].img_x + 1, tp[1].img_y + 1); */

    xastir_snprintf(tigertmp, sizeof(tigertmp), "http://tiger.census.gov/cgi-bin/mapper/map.gif?");

    if (tiger_show_grid)
        strcat(tigertmp, "&on=GRID");
    else
        strcat(tigertmp, "&off=GRID");

    if (tiger_show_counties)
        strcat(tigertmp, "&on=counties");
    else
        strcat(tigertmp, "&off=counties");

    if (tiger_show_cities)
        strcat(tigertmp, "&on=CITIES");
    else
        strcat(tigertmp, "&off=CITIES");

    if (tiger_show_places)
        strcat(tigertmp, "&on=places");
    else
        strcat(tigertmp, "&off=places");

    if (tiger_show_majroads)
        strcat(tigertmp, "&on=majroads");
    else
        strcat(tigertmp, "&off=majroads");

    if (tiger_show_streets)
        strcat(tigertmp, "&on=streets");
    // Don't turn streets off since this will automagically show up as you zoom in.

    if (tiger_show_railroad)
        strcat(tigertmp, "&on=railroad");
    else
        strcat(tigertmp, "&off=railroad");

    if (tiger_show_states)
        strcat(tigertmp, "&on=states");
    else
        strcat(tigertmp, "&off=states");

    if (tiger_show_interstate)
        strcat(tigertmp, "&on=interstate");
    else
        strcat(tigertmp, "&off=interstate");

    if (tiger_show_ushwy)
        strcat(tigertmp, "&on=ushwy");
    else
        strcat(tigertmp, "&off=ushwy");

    if (tiger_show_statehwy)
        strcat(tigertmp, "&on=statehwy");
    else
        strcat(tigertmp, "&off=statehwy");

    if (tiger_show_water)
        strcat(tigertmp, "&on=water");
    else
        strcat(tigertmp, "&off=water");

    if (tiger_show_lakes)
        strcat(tigertmp, "&on=shorelin");
    else
        strcat(tigertmp, "&off=shorelin");

    if (tiger_show_misc)
        strcat(tigertmp, "&on=miscell");
    else
        strcat(tigertmp, "&off=miscell");

    xastir_snprintf(tmpstr, sizeof(tmpstr), "&lat=%f\046lon=%f\046", lat_center, long_center);    
    strcat (tigertmp, tmpstr);
    xastir_snprintf(tmpstr, sizeof(tmpstr), "wid=%f\046ht=%f\046", map_width, map_height);
    strcat (tigertmp, tmpstr);
    xastir_snprintf(tmpstr, sizeof(tmpstr), "iwd=%i\046iht=%i", tp[1].img_x + 1, tp[1].img_y + 1);
    strcat (tigertmp, tmpstr);
    xastir_snprintf(fileimg, sizeof(fileimg), tigertmp);

    if (debug_level & 512) {
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

    xastir_snprintf(local_filename,
        sizeof(local_filename),
        "%s/map.%s",
        get_user_base_dir("tmp"),
        "gif");


    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        // Update to screen
        (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
        return;
    }


#ifdef HAVE_LIBCURL
    curl = curl_easy_init();

    if (curl) { 

        /* verbose debug is keen */
      //        curl_easy_setopt(curl, CURLOPT_VERBOSE, TRUE);
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
        fprintf(stderr,"Couldn't download the Tigermap image\n");
        return;
    }
#else
#ifdef HAVE_WGET
    xastir_snprintf(tempfile, sizeof(tempfile),
        "%s --server-response --timestamping --tries=1 --timeout=%d --output-document=%s \'%s\' 2> /dev/null\n",
        WGET_PATH,
        tigermap_timeout,
        local_filename,
        fileimg);

    if (debug_level & 512)
       fprintf(stderr,"%s",tempfile);

    if (system(tempfile)) {   // Go get the file
       fprintf(stderr,"Couldn't download the Tigermap image\n");
       return;
    }
#else   // HAVE_WGET
        fprintf(stderr,"libcurl or 'wget' not installed.  Can't download image\n");
#endif  // HAVE_WGET
#endif  // HAVE_LIBCURL


    // For debugging the MagickError/MagickWarning segfaults.
    //system("cat /dev/null >/var/tmp/xastir_hacker_map.gif");


    // Set permissions on the file so that any user can overwrite it.
    chmod(local_filename, 0666);

    strcpy(file,local_filename);  // Tell ImageMagick where to find it

    GetExceptionInfo(&exception);

    image_info=CloneImageInfo((ImageInfo *) NULL);
    (void) strcpy(image_info->filename, file);

    if (debug_level & 512) {
           fprintf(stderr,"Copied %s into image info.\n", file);
           fprintf(stderr,"image_info got: %s\n", image_info->filename);
           fprintf(stderr,"Entered ImageMagick code.\n");
           fprintf(stderr,"Attempting to open: %s\n", image_info->filename);
    }

    // We do a test read first to see if the file exists, so we
    // don't kill Xastir in the ReadImage routine.
    f = fopen (image_info->filename, "r");
    if (f == NULL) {
        if (debug_level & 512)
            fprintf(stderr,"File could not be read\n");
        return;
    }
    (void)fclose (f);


    image = ReadImage(image_info, &exception);

    if (image == (Image *) NULL) {
        MagickWarning(exception.severity, exception.reason, exception.description);
        //fprintf(stderr,"MagickWarning\n");
        return;
    }


    if (debug_level & 512)
        fprintf(stderr,"Color depth is %i \n", (int)image->depth);

    if (image->colorspace != RGBColorspace) {
        fprintf(stderr,"TBD: I don't think we can deal with colorspace != RGB");
        if (image)
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    width = image->columns;
    height = image->rows;

    //  Code to mute the image so it's not as bright.
    if (tigermap_intensity != 100) {
        char tempstr[30];

        if (debug_level & 512)
            fprintf(stderr,"level=%s\n", tempstr);

        xastir_snprintf(tempstr, sizeof(tempstr), "%d, 100, 100", tigermap_intensity);
        ModulateImage(image, tempstr);
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


    pixel_pack = GetImagePixels(image, 0, 0, image->columns, image->rows);
    if (!pixel_pack) {
        fprintf(stderr,"pixel_pack == NULL!!!");
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


    if (image->storage_class == PseudoClass && image->colors <= 256) {
        for (l = 0; l < (int)image->colors; l++) {
            // Need to check how to do this for ANY image, as ImageMagick can read in all sorts
            // of image files
            temp_pack = image->colormap[l];
            if (debug_level & 512)
                fprintf(stderr,"Colormap color is %i  %i  %i \n",
                       temp_pack.red, temp_pack.green, temp_pack.blue);

            // Here's a tricky bit:  PixelPacket entries are defined as Quantum's.  Quantum
            // is defined in /usr/include/magick/image.h as either an unsigned short or an
            // unsigned char, depending on what "configure" decided when ImageMagick was installed.
            // We can determine which by looking at MaxRGB or QuantumDepth.
            //
            if (QuantumDepth == 16) {   // Defined in /usr/include/magick/image.h
                if (debug_level & 512)
                    fprintf(stderr,"Color quantum is [0..65535]\n");
                my_colors[l].red   = temp_pack.red;
                my_colors[l].green = temp_pack.green;
                my_colors[l].blue  = temp_pack.blue;
            }
            else {  // QuantumDepth = 8
                if (debug_level & 512)
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

            if (debug_level & 512)
                fprintf(stderr,"Color allocated is %li  %i  %i  %i \n", my_colors[l].pixel,
                       my_colors[l].red, my_colors[l].blue, my_colors[l].green);
        }
    }



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
    if (tp_c_dx < 0) 
        tp_c_dx = -tp_c_dx;       // New  width between tiepoints
    if (tp_c_dy < 0) 
        tp_c_dy = -tp_c_dy;       // New height between tiepoints

    // Calculate step size per pixel
    map_c_dx = ((double) tp_c_dx / abs(tp[1].img_x - tp[0].img_x));
    map_c_dy = ((double) tp_c_dy / abs(tp[1].img_y - tp[0].img_y));

    // Scaled screen step size for use with XFillRectangle below
    scr_dx = (int) (map_c_dx / scale_x) + 1;
    scr_dy = (int) (map_c_dy / scale_y) + 1;

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

    if (debug_level & 512) {
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

    map_c_yc = (tp[0].y_lat + tp[1].y_lat) / 2;     // vert center of map as reference
    map_y_ctr = (long)(height / 2 +0.499);
    scale_x0 = get_x_scale(0,map_c_yc,scale_y);     // reference scaling at vert map center

    map_c_xc  = (tp[0].x_long + tp[1].x_long) / 2;  // hor center of map as reference
    map_x_ctr = (long)(width  / 2 +0.499);
    scr_x_mc  = (map_c_xc - x_long_offset) / scale_x; // screen coordinates of map center

    // calculate map pixel range in y direction that falls into screen area
    c_y_max = 0ul;
    map_y_min = map_y_max = 0l;
    for (map_y_0 = 0, c_y = tp[0].y_lat; map_y_0 < (long)height; map_y_0++, c_y += map_c_dy) {
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
    c_y_min = (unsigned long)(tp[0].y_lat + map_y_min * map_c_dy);   // top map inside screen coordinate

        map_x_min = map_x_max = 0l;
        for (map_x = 0, c_x = tp[0].x_long; map_x < (long)width; map_x++, c_x += map_c_dx) {
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

    scr_yp = -1;
    scr_c_xr = x_long_offset + screen_width * scale_x;
    c_dx = map_c_dx;                            // map pixel width
    scale_xa = scale_x0;                        // the compiler likes it ;-)

    map_done = 0;
    map_act  = 0;
    map_seen = 0;
    scr_y = screen_height - 1;


    // loop over map pixel rows
    for (map_y_0 = map_y_min, c_y = (double)c_y_min; (map_y_0 <= map_y_max); map_y_0++, c_y += map_c_dy) {

        scr_y = (c_y - y_lat_offset) / scale_y;
        if (scr_y != scr_yp) {                  // don't do a row twice
            scr_yp = scr_y;                     // remember as previous y
            scr_xp = -1;
            // loop over map pixel columns
            map_act = 0;
            scale_x_nm = calc_dscale_x(0,(long)c_y) / 1852.0;  // nm per Xastir coordinate
            for (map_x = map_x_min, c_x = (double)c_x_min; map_x <= map_x_max; map_x++, c_x += c_dx) {
                scr_x = (c_x - x_long_offset) / scale_x;
                if (scr_x != scr_xp) {      // don't do a pixel twice
                    scr_xp = scr_x;         // remember as previous x
                    map_y = map_y_0;

                    if (map_y >= 0 && map_y <= tp[1].img_y) { // check map boundaries in y direction
                        map_seen = 1;
                        map_act = 1;    // detects blank screen rows (end of map)

                        // now copy a pixel from the map image to the screen
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
                        (void)XFillRectangle (XtDisplay (w),pixmap,gc,scr_x,scr_y,scr_dx,scr_dy);
                    } // check map boundaries in y direction
                }
            } // loop over map pixel columns
            if (map_seen && !map_act)
                map_done = 1;
        }
    } // loop over map pixel rows

    if (image)
       DestroyImage(image);
    if (image_info)
       DestroyImageInfo(image_info);
}
#endif //HAVE_IMAGEMAGICK
///////////////////////////////////////////// End of Tigermap code ///////////////////////////////////////



