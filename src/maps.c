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

#ifdef HAVE_LIBGEOTIFF
//#include "cpl_csv.h"
#include "xtiffio.h"
#include "geotiffio.h"
#include "geo_normalize.h"
#include "projects.h"
#endif // HAVE_LIBGEOTIFF

#ifdef HAVE_LIBSHP
#ifdef HAVE_SHAPEFIL_H
#include <shapefil.h>
#else
#ifdef HAVE_LIBSHP_SHAPEFIL_H
#include <libshp/shapefil.h>
#else
#error HAVE_LIBSHP defined but no corresponding include defined
#endif // HAVE_LIBSHP_SHAPEFIL_H
#endif // HAVE_SHAPEFIL_H
#endif // HAVE_LIBSHP

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

struct FtpFile {
  char *filename;
  FILE *stream;
};
#endif

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

#define DOS_HDR_LINES 8
#define GRID_MORE 5000


#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }


// Check for XPM and/or ImageMagick.  We use "NO_GRAPHICS"
// to disable some routines below if the support for them
// is not compiled in.
#if !(defined(HAVE_LIBXPM) || defined(HAVE_LIBXPM_IN_XM) || defined(HAVE_IMAGEMAGICK))
  #define NO_GRAPHICS 1
#endif  // !(HAVE_LIBXPM || HAVE_LIBXPM_IN_XM || HAVE_IMAGEMAGICK)


// Print options
Widget print_properties_dialog = (Widget)NULL;
static xastir_mutex print_properties_dialog_lock;
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

time_t last_snapshot = 0;               // Used to determine when to take next snapshot
int doing_snapshot = 0;



typedef struct {
    int img_x;
    int img_y;
    unsigned long x_long;
    unsigned long y_lat;
} tiepoint;


int mag;
int npoints;    /* number of points in a line */


/* MAP pointers */

map_vectors *map_vectors_ptr;
text_label *map_text_label_ptr;
symbol_label *map_symbol_label_ptr;

/* MAP counters */

long vectors_num;
long text_label_num;
long object_label_num;

float geotiff_map_intensity = 0.65;    // Geotiff map color intensity, set from Maps->Geotiff Map Intensity
float imagemagick_gamma_adjust = 0.0;  // Additional imagemagick map gamma correction, set from Maps->Adjust Gamma

// Storage for the index file timestamp
time_t map_index_timestamp;
extern int index_retrieve(char *filename, unsigned long *bottom,
    unsigned long *top, unsigned long *left, unsigned long *right,
    int *map_layer, int *draw_filled, int *auto_maps);
static int map_onscreen_index(char *filename);
extern void index_update_directory(char *directory);
extern void index_update_accessed(char *filename);
 


int grid_size = 0;




void maps_init(void)
{
    init_critical_section( &print_properties_dialog_lock );
}





/*
 *  Calculate NS distance scale at a given location
 *  in meters per Xastir unit
 */
double calc_dscale_y(long x, long y) {

    // approximation by looking at +/- 0.5 minutes offset
    (void)(calc_distance(y-3000, x, y+3000, x)/6000.0);
    // but this scale is fixed at 1852/6000
    return((double)(1852.0/6000.0));
}





/*
 *  Calculate EW distance scale at a given location
 *  in meters per Xastir unit
 */
double calc_dscale_x(long x, long y) {

    // approximation by looking at +/- 0.5 minutes offset
    // we should find a better formula...
    return(calc_distance(y, x-3000, y, x+3000)/6000.0);
}


/*
 *  Calculate x map scaling for current location
 *  With that we could have equal distance scaling or a better
 *  view for pixel maps
 */
long get_x_scale(long x, long y, long ysc) {
    long   xsc;
    double sc_x;
    double sc_y;
    
    sc_x = calc_dscale_x(x,y);          // meter per Xastir unit
    sc_y = calc_dscale_y(x,y);
    if (sc_x < 0.01 || ysc > 50000)
        // keep it near the poles (>88 deg) or if big parts of world seen
        xsc = ysc;
    else
        // adjust y scale, so that the distance is identical in both directions:
        xsc = (long)(ysc * sc_y / sc_x +0.4999);
    
    //fprintf(stderr,"Scale: x %5.3fkm/deg, y %5.3fkm/deg, x %ld y %ld\n",sc_x*360,sc_y*360,xsc,ysc);
    return(xsc);
}





/***********************************************************
 * convert_to_xastir_coordinates()
 *
 * Converts from lat/lon to Xastir coordinate system.
 * First two parameters are the output Xastir X/Y values, 
 * 2nd two are the input floating point lat/lon values.
 *
 *              0 (90 deg. or 90N)
 *
 * 0 (-180 deg. or 180W)      129,600,000 (180 deg. or 180E)
 *
 *          64,800,000 (-90 deg. or 90S)
 ***********************************************************/
int convert_to_xastir_coordinates ( unsigned long* x,
                                    unsigned long* y,
                                    float f_longitude,
                                    float f_latitude )
{
    int ok = 1;


    if (f_longitude < -180.0) {
        fprintf(stderr,"convert_to_xastir_coordinates:Longitude out-of-range (too low):%f\n",f_longitude);
        ok = 0;
    }

    if (f_longitude >  180.0) {
        fprintf(stderr,"convert_to_xastir_coordinates:Longitude out-of-range (too high):%f\n",f_longitude);
        ok = 0;
    }

    if (f_latitude <  -90.0) {
        fprintf(stderr,"convert_to_xastir_coordinates:Latitude out-of-range (too low):%f\n",f_latitude);
        ok = 0;
    }

    if (f_latitude >   90.0) {
        fprintf(stderr,"convert_to_xastir_coordinates:Latitude out-of-range (too high):%f\n",f_latitude);
        ok = 0;
    }

    if (ok) {
        *y = (unsigned long)(32400000l + (360000.0 * (-f_latitude)));
        *x = (unsigned long)(64800000l + (360000.0 * f_longitude));
    }

    return(ok);
}



/** MAP DRAWING ROUTINES **/


/**********************************************************
 * draw_grid()
 *
 * Draws a lat/lon grid on top of the view.
 **********************************************************/
void draw_grid(Widget w) {
    int coord;
    unsigned char dash[2];

    if (!long_lat_grid)
        return;

    /* Set the line width in the GC */
    (void)XSetForeground (XtDisplay (w), gc, colors[0x08]);
    (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineOnOffDash, CapButt,JoinMiter);

    if (0 /*coordinate_system == USE_UTM*/) {
        // Not yet, just teasing... ;-)
    }
    else { // Not UTM coordinate system, draw some lat/long lines
        unsigned int x,x1,x2;
        unsigned int y,y1,y2;
        unsigned int stepsx[3];
        unsigned int stepsy[3];
        int step;
        stepsx[0] = 72000*100;    stepsy[0] = 36000*100;
        stepsx[1] =  7200*100;    stepsy[1] =  3600*100;
        stepsx[2] =   300*100;    stepsy[2] =   150*100;

        //fprintf(stderr,"scale_x: %ld\n",scale_x);
        step = 0;
        if (scale_x <= 6000) step = 1;
        if (scale_x <= 300)  step = 2;

        step += grid_size;
        if (step < 0) {
            grid_size -= step;
            step = 0;
        }
        else if (step > 2) {
            grid_size -= (step - 2);
            step = 2;
        }

        /* draw vertival lines */
        if (y_lat_offset >= 0)
            y1 = 0;
        else
            y1 = -y_lat_offset/scale_y;

        y2 = (180*60*60*100-y_lat_offset)/scale_y;

        if (y2 > (unsigned int)screen_height)
            y2 = screen_height-1;

        coord = x_long_offset+stepsx[step]-(x_long_offset%stepsx[step]);
        if (coord < 0)
            coord = 0;

        for (; coord < x_long_offset+screen_width*scale_x && coord <= 360*60*60*100; coord += stepsx[step]) {

            x = (coord-x_long_offset)/scale_x;

            if ((coord%(648000*100)) == 0) {
                (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineSolid, CapButt,JoinMiter);
                (void)XDrawLine (XtDisplay (w), pixmap_final, gc, x, y1, x, y2);
                (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineOnOffDash, CapButt,JoinMiter);
                continue;
            }
	    else if ((coord%(72000*100)) == 0) {
                dash[0] = dash[1] = 8;
                (void)XSetDashes (XtDisplay (w), gc, 0, dash, 2);
            } else if ((coord%(7200*100)) == 0) {
                dash[0] = dash[1] = 4;
                (void)XSetDashes (XtDisplay (w), gc, 0, dash, 2);
            } else if ((coord%(300*100)) == 0) {
                dash[0] = dash[1] = 2;
                (void)XSetDashes (XtDisplay (w), gc, 0, dash, 2);
            }

            (void)XDrawLine (XtDisplay (w), pixmap_final, gc, x, y1, x, y2);
        }

        /* draw horizontal lines */
        if (x_long_offset >= 0)
            x1 = 0;
        else
            x1 = -x_long_offset/scale_x;

        x2 = (360*60*60*100-x_long_offset)/scale_x;
        if (x2 > (unsigned int)screen_width)
            x2 = screen_width-1;

        coord = y_lat_offset+stepsy[step]-(y_lat_offset%stepsy[step]);
        if (coord < 0)
            coord = 0;

        for (; coord < y_lat_offset+screen_height*scale_y && coord <= 180*60*60*100; coord += stepsy[step]) {

            y = (coord-y_lat_offset)/scale_y;

            if ((coord%(324000*100)) == 0) {
                (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineSolid, CapButt,JoinMiter);
                (void)XDrawLine (XtDisplay (w), pixmap_final, gc, x1, y, x2, y);
                (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineOnOffDash, CapButt,JoinMiter);
                continue;
            } else if ((coord%(36000*100)) == 0) {
                dash[0] = dash[1] = 8;
                (void)XSetDashes (XtDisplay (w), gc, 4, dash, 2);
            } else if ((coord%(3600*100)) == 0) {
                dash[0] = dash[1] = 4;
                (void)XSetDashes (XtDisplay (w), gc, 2, dash, 2);
            } else if ((coord%(150*100)) == 0) {
                dash[0] = dash[1] = 2;
                (void)XSetDashes (XtDisplay (w), gc, 1, dash, 2);
            }

            (void)XDrawLine (XtDisplay (w), pixmap_final, gc, x1, y, x2, y);
        }
    }
}





/**********************************************************
 * map_plot()
 *
 * Plots vectors on the map.  If "color" is non-zero,
 * then it draws filled polygons in the color of
 * "object_behavior"?  Weird.
 **********************************************************/
void map_plot (Widget w, long max_x, long max_y, long x_long_cord,
        long y_lat_cord, unsigned char color, long object_behavior,
        int destination_pixmap, int draw_filled) {

    static int redraw_check;
    static XPoint points[MAX_MAP_POINTS];
    static unsigned char last_color = (unsigned char)0;
    static unsigned char last_behavior = (unsigned char)0, first_behavior = (unsigned char)0;
    long x, y;
    int draw_ok;
    unsigned char line_behavior, fill_color;
    char warning[200];

    /* don't ever go over MAX_MAP_POINTS have a bad map not a crashed program */
    if (npoints > MAX_MAP_POINTS) {
        xastir_snprintf(warning, sizeof(warning), "Warning line point count overflow: map_plot\b\n");
        XtAppWarning (app_context, warning);
        npoints = MAX_MAP_POINTS;
    }

    /* if map_color_levels are on see if we should draw the line? */
    draw_ok = 0;
    if (map_color_levels)   // Decide which colors to display at this zoom level
        switch (color) {
            case (0x01):
            case (0x14):
            case (0x18):
                if (mag < 100)
                    draw_ok = 1;
                break;
            case (0x15):
            case (0x19):
                if (mag < 600)
                    draw_ok = 1;
                break;
            case (0x16):
                if (mag < 800)
                    draw_ok = 1;
                break;
            default:
                draw_ok = 1;
                break;
        }
    else    // Display all colors
        draw_ok = 1;

    if (draw_ok) {
        x = ((x_long_cord - x_long_offset) / scale_x);
        y = ((y_lat_cord  - y_lat_offset)  / scale_y);
        if (x < -MAX_OUTBOUND)
            x = -MAX_OUTBOUND;

        if (y < -MAX_OUTBOUND)
            y = -MAX_OUTBOUND;

        if (x > max_x)
            x = max_x;

        if (y > max_y)
            y = max_y;

        if (debug_level & 16)
            fprintf(stderr," MAP Plot - max_x: %ld, max_y: %ld, x: %ld, y: %ld, color: %d, behavior: %lx, points: %d\n",
                    max_x, max_y, x, y, (int)color, (unsigned long)object_behavior, npoints);

        if ( (last_color != color) || (color == (unsigned char)0xff) ) {
            if (npoints && (last_color != (unsigned char)0xff) ) {
                line_behavior = last_behavior;
                if (last_behavior & 0x80) {
                    if (color) {
                        fill_color = (last_behavior & ~0x80) + (unsigned char)0x60;
                        if (fill_color > (unsigned char)0x69)
                            fill_color = (unsigned char)0x60;
                    } else
                        fill_color = (unsigned char)object_behavior;


                    // Here's where we draw filled areas using fill_color.

                    (void)XSetForeground (XtDisplay (w), gc, colors[(int)fill_color]);

                    switch (destination_pixmap) {

                    case DRAW_TO_PIXMAP:
                        // We must be drawing maps 'cuz this is the pixmap we use for it.
                        if (map_color_fill && draw_filled) {

                            if (npoints >= 3) {
                                (void)XFillPolygon(XtDisplay(w),
                                    pixmap,
                                    gc,
                                    points,
                                    npoints,
                                    Nonconvex,
                                    CoordModeOrigin);
                            }
                            else {
                                fprintf(stderr,
                                    "map_plot:Too few points:%d, Skipping XFillPolygon()",
                                    npoints);
                            }
                        }
                        break;

                    case DRAW_TO_PIXMAP_ALERTS:
                        fprintf(stderr,"You're calling the wrong routine to draw weather alerts!\n");
                        break;

                    case DRAW_TO_PIXMAP_FINAL:
                        // We must be drawing symbols/tracks 'cuz this is the pixmap we use for it.

                        if (npoints >= 3) {
                            (void)XFillPolygon(XtDisplay(w),
                                pixmap_final,
                                gc,
                                points,
                                npoints,
                                Nonconvex,
                                CoordModeOrigin);
                        }
                        else {
                            fprintf(stderr,
                                "map_plot:Too few points:%d, Skipping XFillPolygon()",
                                npoints);
                        }
                        break;
                    }

                    line_behavior = first_behavior;
                }
                if (line_behavior & 0x01)
                    (void)XSetLineAttributes (XtDisplay (w), gc, 2, LineSolid, CapButt,JoinMiter);
                else
                    (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineSolid, CapButt,JoinMiter);

                if (color == (unsigned char)0x56)
                    (void)XSetLineAttributes (XtDisplay (w), gc, 10, LineSolid, CapButt,JoinMiter);

                // Set the color for drawing lines/borders
                (void)XSetForeground (XtDisplay (w), gc, colors[(int)last_color]);

                switch (destination_pixmap) {

                case DRAW_TO_PIXMAP_FINAL:
                    (void)XDrawLines (XtDisplay (w), pixmap_final, gc, points, npoints,CoordModeOrigin);
                    break;

                case DRAW_TO_PIXMAP:
                    (void)XDrawLines (XtDisplay (w), pixmap, gc, points, npoints,CoordModeOrigin);
                    break;

                case DRAW_TO_PIXMAP_ALERTS:
                    fprintf(stderr,"You're calling the wrong routine to draw weather alerts!\n");
                    break;
                }

                npoints = 0;

                /* check to see if we have been away from the screen too long */
                if (redraw_check > 1000) {
                    redraw_check = 0;
                    XmUpdateDisplay (XtParent (da));
                }
                redraw_check++;
            }
            last_color = color;
            if (color == (unsigned char)0xff) {
                npoints = 0;
                first_behavior = (unsigned char)object_behavior;
            }
            points[npoints].x = (short)x;
            points[npoints].y = (short)y;
            if (    (points[npoints].x > (-MAX_OUTBOUND))
                 && (points[npoints].x < (short)max_x)
                 && (points[npoints].y > (-MAX_OUTBOUND))
                 && (points[npoints].y < (short)max_y)
                 && (color != (unsigned char)0) )

                npoints++;

            last_behavior = (unsigned char)object_behavior;
            return;
        }
        points[npoints].x = (short)x;
        points[npoints].y = (short)y;
        last_behavior = (unsigned char)object_behavior;

        if (points[npoints].x != points[npoints - 1].x || points[npoints].y != points[npoints - 1].y) {
            if (last_behavior & 0x80)
                npoints++;

            else if (points[npoints].x > (-MAX_OUTBOUND)
                     && points[npoints].x < (short)max_x
                     && points[npoints].y > (-MAX_OUTBOUND)
                     && points[npoints].y < (short)max_y)
                npoints++;

        }
    } else {
        npoints = 0;
    }
}   /* map_plot */





/**********************************************************
 * get_map_ext()
 *
 * Returns the extension for the filename.  We use this to
 * determine which sort of map file it is.
 **********************************************************/
char *get_map_ext (char *filename) {
    int len;
    int i;
    char *ext;

    ext = NULL;
    len = (int)strlen (filename);
    for (i = len; i >= 0; i--) {
        if (filename[i] == '.') {
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
char *get_map_dir (char *fullpath) {
    int len;
    int i;

    len = (int)strlen (fullpath);
    for (i = len; i >= 0; i--) {
        if (fullpath[i] == '/') {
            fullpath[i + 1] = '\0';
            break;
        }
    }
    return (fullpath);
}





/**********************************************************
 * get_alt_fgd_path()
 *
 * Used to search for .fgd in ../metadata subdir, as it is
 * layed out on a USGS CDROM.
 **********************************************************/
void get_alt_fgd_path(char *fullpath, int fullpath_length) {
    int len;
    int i, j = 0;
    char *dir = fullpath;
    char fname[MAX_FILENAME];

    // Split up into directory and filename
    len = (int)strlen (fullpath);
    for (i = len; i >= 0; i--) {
        if (fullpath[i] == '/') {
            dir = &fullpath[i];
            break;
        }
    }
    for (++i; i <= len; i++) {
    fname[j++] = fullpath[i];   // Grab the filename
    if (fullpath[i] == '\0')
        break;
    }

    // We have the filename now.  dir now points to
    // the '/' at the end of the path.

    // Now do it again to knock off the "data" subdirectory
    // from the end.
    dir[0] = '\0';  // Terminate the current string, wiping out the '/' character
    len = (int)strlen (fullpath);   // Length of the new shortened string
    for (i = len; i >= 0; i--) {
        if (fullpath[i] == '/') {
            dir = &fullpath[i + 1]; // Dir now points to one past the '/' character
            break;
        }
    }
    for (++i; i <= len; i++) {
    if (fullpath[i] == '\0')
        break;
    }   

    // Add "metadata/" into the path
    xastir_snprintf(dir, fullpath_length, "metadata/%s", fname);
    //fprintf(stderr,"FGD Directory: %s\n", fullpath);
}





/**********************************************************
 * get_alt_fgd_path2()
 *
 * Used to search for .fgd in Metadata subdir.  This function
 * is no longer used.
 **********************************************************/
void get_alt_fgd_path2(char *fullpath, int fullpath_length) {
    int len;
    int i, j = 0;
    char *dir = fullpath;
    char fname[MAX_FILENAME];

    // Split up into directory and filename
    len = (int)strlen (fullpath);
    for (i = len; i >= 0; i--) {
        if (fullpath[i] == '/') {
            dir = &fullpath[i + 1];
            break;
        }
    }
    for (++i; i <= len; i++) {
    fname[j++] = fullpath[i];
    if (fullpath[i] == '\0')
        break;
    }

    // Add "Metadata/" into the path
    xastir_snprintf(dir, fullpath_length, "Metadata/%s", fname);
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
 * Skipping map: /usr/local/xastir/maps/tif/uk/425_0525_bng.tif
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
int map_visible (unsigned long bottom_map_boundary,
                    unsigned long top_map_boundary,
                    unsigned long left_map_boundary,
                    unsigned long right_map_boundary)
{

    unsigned long view_min_x, view_max_x;
    unsigned long view_min_y, view_max_y;
    int map_inside_view = 0;
    int view_inside_map = 0;
    int parallel_edges = 0;

    view_min_x = (unsigned long)x_long_offset;                         /*   left edge of view */
    if (view_min_x > 129600000ul)
        view_min_x = 0;

    view_max_x = (unsigned long)(x_long_offset + (screen_width * scale_x)); // right edge of view
    if (view_max_x > 129600000ul)
        view_max_x = 129600000ul;

    view_min_y = (unsigned long)y_lat_offset;                          /*    top edge of view */
    if (view_min_y > 64800000ul)
        view_min_y = 0;

    view_max_y = (unsigned long)(y_lat_offset + (screen_height * scale_y)); // bottom edge of view
    if (view_max_y > 64800000ul)
        view_max_y = 64800000ul;

    if (debug_level & 16) {
        fprintf(stderr,"              Bottom     Top       Left     Right\n");

        fprintf(stderr,"View Edges:  %lu  %lu  %lu  %lu\n",
            view_max_y,
            view_min_y,
            view_min_x,
            view_max_x);

        fprintf(stderr," Map Edges:  %lu  %lu  %lu  %lu\n",
            bottom_map_boundary,
            top_map_boundary,
            left_map_boundary,
            right_map_boundary);

        if ((left_map_boundary <= view_max_x) && (left_map_boundary >= view_min_x))
            fprintf(stderr,"Left map boundary inside view\n");

        if ((right_map_boundary <= view_max_x) && (right_map_boundary >= view_min_x))
            fprintf(stderr,"Right map boundary inside view\n");

        if ((top_map_boundary <= view_max_y) && (top_map_boundary >= view_min_y))
            fprintf(stderr,"Top map boundary inside view\n");

        if ((bottom_map_boundary <= view_max_y) && (bottom_map_boundary >= view_min_y))
            fprintf(stderr,"Bottom map boundary inside view\n");

        if ((view_max_x <= right_map_boundary) && (view_max_x >= left_map_boundary))
            fprintf(stderr,"Right view boundary inside map\n");

        if ((view_min_x <= right_map_boundary) && (view_min_x >= left_map_boundary))
            fprintf(stderr,"Left view boundary inside map\n");

        if ((view_max_y <= bottom_map_boundary) && (view_max_y >= top_map_boundary))
            fprintf(stderr,"Bottom view boundary inside map\n");

        if ((view_min_y <= bottom_map_boundary) && (view_min_y >= top_map_boundary))
            fprintf(stderr,"Top view boundary inside map\n");
    }


    /* In order to determine whether the two rectangles intersect,
    * we need to figure out if any TWO edges of one rectangle are
    * contained inside the edges of the other.
    */

    /* Look for left or right map boundaries inside view */
    if (   (( left_map_boundary <= view_max_x) && ( left_map_boundary >= view_min_x)) ||
            ((right_map_boundary <= view_max_x) && (right_map_boundary >= view_min_x)))
    {
        map_inside_view++;
    }


    /* Look for top or bottom map boundaries inside view */
    if (   ((   top_map_boundary <= view_max_y) && (   top_map_boundary >= view_min_y)) ||
            ((bottom_map_boundary <= view_max_y) && (bottom_map_boundary >= view_min_y)))
    {

        map_inside_view++;
    }


    /* Look for right or left view boundaries inside map */
    if (   ((view_max_x <= right_map_boundary) && (view_max_x >= left_map_boundary)) ||
            ((view_min_x <= right_map_boundary) && (view_min_x >= left_map_boundary)))
    {
        view_inside_map++;
    }


    /* Look for top or bottom view boundaries inside map */
    if (   ((view_max_y <= bottom_map_boundary) && (view_max_y >= top_map_boundary)) ||
        ((view_min_y <= bottom_map_boundary) && (view_min_y >= top_map_boundary)))
    {
        view_inside_map++;
    }


    /*
    * Look for left/right map boundaries both inside view, but top/bottom
    * of map surround the viewport.  We have a column of the map going
    * through from top to bottom.
    */
    if (   (( left_map_boundary <= view_max_x) && ( left_map_boundary >= view_min_x)) &&
            ((right_map_boundary <= view_max_x) && (right_map_boundary >= view_min_x)) &&
            ((view_max_y <= bottom_map_boundary) && (view_max_y >= top_map_boundary)) &&
            ((view_min_y <= bottom_map_boundary) && (view_min_y >= top_map_boundary)))
    {
        parallel_edges++;
    }


    /*
    * Look for top/bottom map boundaries both inside view, but left/right
    * of map surround the viewport.  We have a row of the map going through
    * from left to right.
    */
    if (   ((   top_map_boundary <= view_max_y) && (   top_map_boundary >= view_min_y)) &&
        ((bottom_map_boundary <= view_max_y) && (bottom_map_boundary >= view_min_y)) &&
        ((view_max_x <= right_map_boundary) && (view_max_x >= left_map_boundary)) &&
        ((view_min_x <= right_map_boundary) && (view_min_x >= left_map_boundary)))
    {
        parallel_edges++;
    }


    if (debug_level & 16)
        fprintf(stderr,"map_inside_view: %d  view_inside_map: %d  parallel_edges: %d\n",
                map_inside_view,
                view_inside_map,
                parallel_edges);

    if ((map_inside_view >= 2) || (view_inside_map >= 2) || (parallel_edges) )
        return (1); /* Draw this pixmap onto the screen */
    else
        return (0); /* Skip this pixmap */
}





/***********************************************************
 * map_visible_lat_lon()
 *
 ***********************************************************/
int map_visible_lat_lon (double f_bottom_map_boundary,
                         double f_top_map_boundary,
                         double f_left_map_boundary,
                         double f_right_map_boundary,
                         char *error_message)
{
    unsigned long bottom_map_boundary,
                  top_map_boundary,
                  left_map_boundary,
                  right_map_boundary;
    int ok, ok2;

    ok = convert_to_xastir_coordinates ( &left_map_boundary,
                                          &top_map_boundary,
                                          (float)f_left_map_boundary,
                                          (float)f_top_map_boundary );

    ok2 = convert_to_xastir_coordinates ( &right_map_boundary,
                                          &bottom_map_boundary,
                                          (float)f_right_map_boundary,
                                          (float)f_bottom_map_boundary );

    if (ok && ok2) {
        return(map_visible( bottom_map_boundary,
                        top_map_boundary,
                        left_map_boundary,
                        right_map_boundary) );
    }
    else {
        fprintf(stderr,"map_visible_lat_lon: problem converting coordinates from lat/lon: %s\n",
            error_message);
        return(0);  // Problem converting, assume that the map is not viewable
    }
}





/**********************************************************
 * draw_label_text()
 *
 * Does what it says.  Used to draw strings onto the
 * display.
 **********************************************************/
void draw_label_text (Widget w, int x, int y, int label_length, int color, char *label_text) {

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
//
static XFontStruct *rotated_label_font=NULL;
char *rotated_label_fontname=
    //"-adobe-helvetica-medium-o-normal--24-240-75-75-p-130-iso8859-1";
    //"-adobe-helvetica-medium-o-normal--12-120-75-75-p-67-iso8859-1";
    //"-adobe-helvetica-medium-r-*-*-*-130-*-*-*-*-*-*";
    //"-*-times-bold-r-*-*-13-*-*-*-*-80-*-*";
    //"-*-helvetica-bold-r-*-*-14-*-*-*-*-80-*-*";
    "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";





/**********************************************************
 * draw_rotated_label_text()
 *
 * Does what it says.  Used to draw strings onto the
 * display.
 *
 * Use "xfontsel" or other tools to figure out what fonts
 * to use here.
 **********************************************************/
void draw_rotated_label_text (Widget w, int rotation, int x, int y, int label_length, int color, char *label_text) {
//    XPoint *corner;
//    int i;
    float my_rotation = (float)((-rotation)-90);


    /* load font */
    if(!rotated_label_font) {
        rotated_label_font=(XFontStruct *)XLoadQueryFont(XtDisplay (w),
                                                rotated_label_fontname);
        if (rotated_label_font == NULL) {	// Couldn't get the font!!!
            fprintf(stderr,"draw_rotated_label_text: Couldn't get font %s\n",
                rotated_label_fontname);
            return;
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

    if (       ( (my_rotation < -90.0) && (my_rotation > -270.0) )
            || ( (my_rotation >  90.0) && (my_rotation <  270.0) ) ) {

        my_rotation = my_rotation + 180.0;
        (void)XRotDrawAlignedString(XtDisplay (w),
            rotated_label_font,
            my_rotation,
            pixmap,
            gc,
            x,
            y,
            label_text,
            BRIGHT);
    }
    else {
        (void)XRotDrawAlignedString(XtDisplay (w),
            rotated_label_font,
            my_rotation,
            pixmap,
            gc,
            x,
            y,
            label_text,
            BLEFT);
    }
}





#ifdef HAVE_LIBSHP
/*******************************************************************
 * create_shapefile_map()
 *
 * Do we have a need for storing date/ time/ speed/ course/
 * altitude/ heard-direct for each point?  Altitude could be stored
 * in the Z space.  If we store a station's trail as an SHPT_POINT
 * file, then we can associate a bunch of info with each point:
 * date/time, altitude, course, speed, status/comments, path, port,
 * heard-direct, weather data, etc.  We could also dynamically
 * change the number of fields based on what we have a need to
 * store, using field-names to determine what's stored in each file.
 *
 * shapefile_name is the path/name of the map file we wish to create
 * (without the extension).
 *
 * type = SHPT_POINT, SHPT_ARC, SHPT_POLYGON,
 * SHPT_MULTIPOINT, etc.
 *
 * quantity equals the number of vertices we have.
 *
 * padfx/padfy/padfz are the vertices themselves, in double format.
 *******************************************************************/
void create_shapefile_map(char *dir, char *shapefile_name, int type,
        int quantity, double *padfx, double *padfy, double *padfz,
        int add_timestamp) {

    SHPHandle my_shp_handle;
    SHPObject *my_object;
    DBFHandle my_dbf_handle;
    char timedatestring[101];
    int index;
    int max_objects = 1;
    char credit_string[] = "Created by Xastir, http://www.xastir.org";
    char temp_shapefile_name[MAX_FILENAME];


    if (debug_level & 16) {
        fprintf(stderr,"create_shapefile_map\n");
        fprintf(stderr,"%s %s %d %d %d\n",
            dir,
            shapefile_name,
            type,
            quantity,
            add_timestamp);
    }

    if (quantity == 0) {
        // No reason to make a map if we don't have any points.
        return;
    }

    // Get the time/datestamp
    get_timestamp(timedatestring);

    if (add_timestamp) {    // Prepend a timestamp to the filename
        int ii;

        xastir_snprintf(temp_shapefile_name,
            sizeof(temp_shapefile_name),
            "%s%s_%s",
            dir,
            timedatestring,
            shapefile_name);

        // Change spaces to underlines
        for (ii = 0; ii < strlen(temp_shapefile_name); ii++) {
            if (temp_shapefile_name[ii] == ' ') {
                temp_shapefile_name[ii] = '_';
            }
        }
    }
    else {  // Use the filename directly, no timestamp
        xastir_snprintf(temp_shapefile_name,
            sizeof(temp_shapefile_name),
            "%s%s",
            dir,
            shapefile_name);
    }

    if (debug_level & 16)
        fprintf(stderr, "Creating file %s\n", temp_shapefile_name);
 
    // Create empty .shp/.shx/.dbf files
    //
    my_shp_handle = SHPCreate(temp_shapefile_name, type);
    my_dbf_handle = DBFCreate(temp_shapefile_name);

    // Check whether we were able to open these handles
    if ((my_shp_handle == NULL) || (my_dbf_handle == NULL)) {
        // Probably write-protected directory
        fprintf(stderr, "Could not create shapefile %s\n",
            temp_shapefile_name);
        return;
    }

    // Create the different fields we'll use to store the
    // attributes:
    //
    // Add a credits field and set the length.  Field 0.
    DBFAddField(my_dbf_handle, "Credits", FTString, strlen(credit_string) + 1, 0);
    //
    // Add a date/time field and set the length.  Field 1.
    DBFAddField(my_dbf_handle, "DateTime", FTString, strlen(timedatestring) + 1, 0);

    // Note that if were passed additional parameters that went
    // along with the lat/long/altitude points, we could write those
    // into the DBF file in the loop below.  We would have to change
    // from a polygon to a point filetype though.

    // Populate the files with objects and attributes.  Perform this
    // loop once for each object.
    //
    for (index = 0; index < max_objects; index++) {

        // Create a temporary object from the vertices
        my_object = SHPCreateSimpleObject( type,
            quantity,
            padfx,
            padfy,
            padfz);

        // Write out the vertices
        SHPWriteObject( my_shp_handle,
            -1,
            my_object);

        // Destroy the temporary object
        SHPDestroyObject(my_object);

// Note that with the current setup the below really only get
// written into the file once.  Check it with dbfinfo/dbfdump.

        // Write the credits attributes
        DBFWriteStringAttribute( my_dbf_handle,
            index,
            0,
            credit_string);

        // Write the time/date string
        DBFWriteStringAttribute( my_dbf_handle,
            index,
            1,
            timedatestring);
    }

    // Close the .shp/.shx/.dbf files
    SHPClose(my_shp_handle);
    DBFClose(my_dbf_handle);
}





// Function which creates a Shapefile map from an APRS trail.
//
// Navigate through the linked list of TrackRow structs to pick out
// the lat/long and write them into arrays of floats.  We then pass
// those arrays to create_shapefile_map().
//
void create_map_from_trail(char *call_sign) {
    DataRow *p_station;


    // Find the station in our database.  Count the number of points
    // for that station first, then allocate some arrays to hold
    // that quantity of points.  Stuff the lat/long into the arrays
    // and then call create_shapefile_map().
    //
    if (search_station_name(&p_station, call_sign, 1)) {
        int count;
        int ii;
        TrackRow *ptr;
        char temp[MAX_FILENAME];
        char temp2[MAX_FILENAME];
        double *x;
        double *y;
        double *z;


        count = 0;
        ptr = p_station->oldest_trackpoint;
        while (ptr != NULL) {
            count++;
            ptr = ptr->next;
        }

//fprintf(stderr, "Quantity of points: %d\n", count);

        if (count == 0) {
            // No reason to make a map if we don't have any points
            // in the track list.
           return;
        }

        // We know how many points are in the linked list.  Allocate
        // arrays to hold the values.
        x = (double *) malloc( count * sizeof(double) );
        y = (double *) malloc( count * sizeof(double) );
        z = (double *) malloc( count * sizeof(double) );
        CHECKMALLOC(x);
        CHECKMALLOC(y);
        CHECKMALLOC(z);

        // Fill in the values.  We need to convert from Xastir
        // coordinate system to lat/long doubles as we go.
        ptr = p_station->oldest_trackpoint;
        ii = 0;
        while ((ptr != NULL) && (ii < count) ) {

            // Convert from Xastir coordinates to lat/long

            // Convert to string
            convert_lon_l2s(ptr->trail_long_pos, temp, sizeof(temp), CONVERT_DEC_DEG);
            // Convert to double and stuff into array of doubles
            (void)sscanf(temp, "%lf", &x[ii]);
            // If longitude string contains "W", make the final
            // result negative.
            if (index(temp, 'W'))
                x[ii] = x[ii] * -1.0;
           
            // Convert to string 
            convert_lat_l2s(ptr->trail_lat_pos, temp, sizeof(temp), CONVERT_DEC_DEG);
            // Convert to double and stuff into array of doubles
            (void)sscanf(temp, "%lf", &y[ii]);
            // If latitude string contains "S", make the final
            // result negative.
            if (index(temp, 'S'))
                y[ii] = y[ii] * -1.0;

            z[ii] = ptr->altitude;  // Altitude (meters), undefined=-99999

            ptr = ptr->next;
            ii++;
        }

        // Create a Shapefile from the APRS trail.  Write it into
        // "/maps/GPS" and add a date/timestamp to the end.
        //
        // Create directory name ("maps/GPS/")
        xastir_snprintf(temp, sizeof(temp),
            "%s/GPS/",
            get_data_base_dir("maps"));

        // Create filename
        xastir_snprintf(temp2, sizeof(temp2),
            "%s%s",
            call_sign,
            "_APRS_Trail_Red");

        create_shapefile_map(
            temp,
            temp2,
            SHPT_ARC,
            count,
            x,
            y,
            z,
            1); // Add a timestamp to the front of the filename

        // Free the storage that we malloc'ed
        free(x);
        free(y);
        free(z);
    }
    else {  // Couldn't find the station of interest
    }
}





// Code borrowed from Shapelib which determines whether a ring is CW
// or CCW.  Thanks to Frank Warmerdam for permitting us to use this
// under the GPL license!  Per e-mail of 04/29/2003 between Frank
// and Curt, WE7U.  Frank gave permission for us to use _any_
// portion of Shapelib inside the GPL'ed Xastir program.
//
// Test Ring for Clockwise/Counter-Clockwise (fill or hole ring)
//
// Return  1  for Clockwise (fill ring)
// Return -1  for Counter-Clockwise (hole ring)
// Return  0  for error/indeterminate (shouldn't get this!)
//
int shape_ring_direction ( SHPObject *psObject, int Ring ) {
    int nVertStart;
    int nVertCount;
    int iVert;
    float dfSum;
    int result;

 
    nVertStart = psObject->panPartStart[Ring];

    if( Ring == psObject->nParts-1 )
        nVertCount = psObject->nVertices - psObject->panPartStart[Ring];
    else
        nVertCount = psObject->panPartStart[Ring+1] 
            - psObject->panPartStart[Ring];

    dfSum = 0.0;
    for( iVert = nVertStart; iVert < nVertStart+nVertCount-1; iVert++ )
    {
        dfSum += psObject->padfX[iVert] * psObject->padfY[iVert+1]
               - psObject->padfY[iVert] * psObject->padfX[iVert+1];
    }

    dfSum += psObject->padfX[iVert] * psObject->padfY[nVertStart]
           - psObject->padfY[iVert] * psObject->padfX[nVertStart];

    if (dfSum < 0.0)
        result = 1;
    else if (dfSum > 0.0)
        result = -1;
    else 
        result = 0;

    return(result);
}




 
/**********************************************************
 * draw_shapefile_map()
 *
 * This function handles both weather-alert shapefiles (from the
 * NOAA site) and shapefiles used as maps (from a number of
 * sources).
 *
 * If destination_pixmap equals INDEX_CHECK_TIMESTAMPS or
 * INDEX_NO_TIMESTAMPS, then we are indexing the file (finding the
 * extents) instead of drawing it.
 *
 * The current implementation can draw Polygon, PolyLine, and Point
 * Shapefiles, but only from a few sources (NOAA, Mapshots.com, and
 * ESRI/GeographyNetwork.com).  We don't handle some of the more
 * esoteric formats.  We now handle the "hole" drawing in polygon
 * shapefiles, where one direction around the ring means a fill, and
 * the other direction means a hole in the polygon.
 *
 * Note that we must currently hard-code the file-recognition
 * portion and the file-drawing portion, because every new source of
 * Shapefiles has a different format, and the fields and field
 * definitions can all change between them.
 *
 * If alert is NULL, draw every shape that fits the screen.  If
 * non-NULL, draw only the shape that matches the zone number.
 * 
 * Here's what I get for the County_Warning_Area Shapefile:
 *
 *
 * Info for shapefiles/county_warning_areas/w_24ja01.shp
 * 4 Columns,  121 Records in file
 *            WFO          string  (3,0)
 *            CWA          string  (3,0)
 *            LON           float  (18,5)
 *            LAT           float  (18,5)
 * Info for shapefiles/county_warning_areas/w_24ja01.shp
 * Polygon(5), 121 Records in file
 * File Bounds: (              0,              0)
 *              (    179.7880249,    71.39809418)
 *
 * From the NOAA web pages:

 Zone Alert Maps: (polygon) (such as z_16mr01.shp)
 ----------------
 field name type   width,dec description 
 STATE      character 2     [ss] State abbrev (US Postal Standard) 
 ZONE       character 3     [zzz] Zone number, from WSOM C-11 
 CWA        character 3     County Warning Area, from WSOM C-47 
 NAME       character 254   Zone name, from WSOM C-11 
 STATE_ZONE character 5     [sszzz] state+("00"+zone.trim).right(3)) 
 TIME_ZONE  character 2     [tt] Time Zone, 2 chars if split 
 FE_AREA    character 2     [aa] Cardinal area of state (occasional) 
 LON        numeric   10,5  Longitude of centroid [decimal degrees] 
 LAT        numeric   9,5   Latitude of centroid [decimal degrees] 


 NOTE:  APRS weather alerts have these sorts of codes in them:
    AL_C001
    AUTAUGACOUNTY
    MS_C075
    LAUDERDALE&NEWTONCOUNTIES
    KY_C009
    EDMONSON
    MS_CLARKE
    MS_C113
    MN_Z076
    PIKECOUNTY
    ATTALACOUNTY
    ST.LUCIECOUNTY
    CW_AGLD

 The strings in the shapefiles are mixed-case, and it appears that the NAME field would
 have the county name, in case state_zone-number format was not used.  We can use the
 STATE_ZONE filed for a match unless it is a non-standard form, in which case we'll need
 to look through the NAME field, and perhaps chop off the "SS_" state portion first.


 County Warning Areas: (polygon)
 ---------------------
 field name type   width,dec description 
 WFO        character 3     WFO Identifier (name of CWA) 
 CWA        character 3     CWA Identifier (same as WFO) 
 LON        numeric   10,5  Longitude of centroid [decimal degrees]
 LAT        numeric   9,5   Latitude of centroid [decimal degrees]

 Coastal and Offshore Marine Areas: (polygon)
 ----------------------------------
 field name type   width,dec description 
 ID         character 6     Marine Zone Identifier 
 WFO        character 3     Assigned WFO (Office Identifier) 
 NAME       character 250   Name of Marine Zone 
 LON        numeric   10,5  Longitude of Centroid [decimal degrees] 
 LAT        numeric   9,5   Latitude of Centroid [decimal degrees] 
 WFO_AREA   character 200   "Official Area of Responsibility", from WSOM D51/D52 

 Road Maps: (polyline)
 ----------
 field name type     width,dec  description
 STFIPS     numeric     2,0     State FIPS Code
 CTFIPS     numeric     3,0     County FIPS Code
 MILES      numeric     6,2     length [mi]
 KILOMETERS numeric     6,2     length [km]
 TOLL       numeric     1,0 
 SURFACE    numeric     1,0     Surface type
 LANES      numeric     2,0     Number of lanes
 FEAT_CLASS numeric     2,0  
 CLASS      character   30
 SIGN1      character   6       Primary Sign Route
 SIGN2      character   6       Secondary Sign Route
 SIGN3      character   6       Alternate Sign Route
 DESCRIPT   character   35      Name of road (sparse)
 SPEEDLIM   numeric     16,0
 SECONDS    numeric     16,2

Lakes (lk17de98.shp):
---------------------
field name  type     width,dec  description
NAME        string      (40,0)
FEATURE     string      (40,0)
LON         float       (10,5)
LAT         float       (9,5)


USGS Quads Overlay (24kgrid.shp) from
http://data.geocomm.com/quadindex/
----------------------------------
field name  type     width,dec  Example
NAME        string     (30,0)   Lummi Bay OE W
STATE       string      (2,0)   WA
LAT         string      (6,0)   48.750
LON         string      (8,0)   -122.750
MRC         string      (8,0)   48122-G7


// Need to figure out which type of alert it is, select the corresponding shapefile,
// then store the shapefile AND the alert_tag in the alert_list[i].filename list?
// and draw the map.  Add an item to alert_list structure to keep track?

// The last parameter denotes loading into pixmap_alerts instead of pixmap or pixmap_final
// Here's the old APRS-type map call:
//map_search (w, alert_scan, alert, &alert_count,(int)(alert_status[i + 2] == DATA_VIA_TNC || alert_status[i + 2] == DATA_VIA_LOCAL), DRAW_TO_PIXMAP_ALERTS);

// Check the zone name(s) to see which Shapefile(s) to use.

            switch (zone[4]) {
                case ('C'): // County File (c_16my01.shp)
                    break;
                case ('A'): // County Warning File (w_24ja01.shp)
                    break;
                case ('Z'): // Zone File (z_16mr01.shp, z_16my01.shp, mz24ja01.shp, oz09de99.shp)
                    break;
            }


 **********************************************************/
void draw_shapefile_map (Widget w,
                        char *dir,
                        char *filenm,
                        alert_entry * alert,
                        char alert_color,
                        int destination_pixmap,
                        int draw_filled) {

    DBFHandle       hDBF;
    SHPObject       *object;
    static XPoint   points[MAX_MAP_POINTS];
    char            file[MAX_FILENAME];  /* Complete path/name of image file */
    char            warning_text[MAX_FILENAME*2];
    int             *panWidth, i, fieldcount, recordcount, structure, ring;
    char            ftype[15];
    int             nWidth, nDecimals;
    SHPHandle       hSHP;
    int             nShapeType, nEntities;
    double          adfBndsMin[4], adfBndsMax[4];
    char            sType [15]= "";
    unsigned long   my_lat, my_long;
    long            x,y;
    int             ok, index;
    int             polygon_hole_flag;
    int             *polygon_hole_storage;
    GC              gc_temp = NULL;
    XGCValues       gc_temp_values;
    Region          region[3];
    int             temp_region1;
    int             temp_region2;
    int             temp_region3;
    int             gps_flag = 0;
    char            gps_label[100];
    int             gps_color = 0x0c;
    int             road_flag = 0;
    int             lake_flag = 0;
    int             river_flag = 0;
    int             railroad_flag = 0;
    int             path_flag = 0;
    int             city_flag = 0;
    int             quad_overlay_flag = 0;
    int             mapshots_labels_flag = 0;
    int             weather_alert_flag = 0;
    char            *filename;  // filename itself w/o directory
    char            search_param1[10];
    int             search_field1 = 0;
    char            search_param2[10];
    int             search_field2 = -1;
    int             found_shape = -1;
    int             start_record;
    int             end_record;
    int             ok_to_draw = 0;
    int             high_water_mark_i = 0;
    int             high_water_mark_index = 0;
    char            quad_label[100];
    char            status_text[MAX_FILENAME];

    typedef struct _label_string {
        char   label[50];
        int    found;
        struct _label_string *next;
    } label_string;

    label_string *label_ptr = NULL;
    label_string *ptr2 = NULL;


    //fprintf(stderr,"*** Alert color: %d ***\n",alert_color);

    // We don't draw the shapes if alert_color == -1
    if (alert_color != -1)
        ok_to_draw++;

    search_param1[0] = '\0';
    search_param2[0] = '\0';

    xastir_snprintf(file, sizeof(file), "%s/%s", dir, filenm);

    //fprintf(stderr,"draw_shapefile_map:start:%s\n",file);

    filename = filenm;
    i = strlen(filenm);
    while ( (filenm[i] != '/') && (i >= 0) )
        filename = &filenm[i--];
        //fprintf(stderr,"draw_shapefile_map:filename:%s\ttitle:%s\n",filename,alert->title);    

    if (alert)
        weather_alert_flag++;

    // Check for maps/GPS directory.  We set up the labels and
    // colors differently for these types of files.
    if (strstr(filenm,"GPS")) { // We're in the maps/GPS directory
        gps_flag++;
    }

    // Open the .dbf file for reading.  This has the textual
    // data (attributes) associated with each shape.
    hDBF = DBFOpen( file, "rb" );
    if ( hDBF == NULL ) {
        if (debug_level & 16)
            fprintf(stderr,"draw_shapefile_map: DBFOpen(%s,\"rb\") failed.\n", file );

        return;
    }

    if (debug_level & 16)
        fprintf(stderr,"\n---------------------------------------------\nInfo for %s\n",filenm);

    fieldcount = DBFGetFieldCount(hDBF);
    if (fieldcount == (int)NULL) {
        DBFClose( hDBF );   // Clean up open file descriptors
        return;     // Should have at least one field
        
    }
    recordcount = DBFGetRecordCount(hDBF);
    if (recordcount == (int)NULL) {
        DBFClose( hDBF );   // Clean up open file descriptors
        return;     // Should have at least one record
    }
    if (debug_level & 16)
        fprintf(stderr,"%d Columns,  %d Records in file\n", fieldcount, recordcount);

    panWidth = (int *) malloc( fieldcount * sizeof(int) );
    CHECKMALLOC(panWidth);
// Make sure to free(panWidth) everywhere we return from!!!

    // If we're doing weather alerts and index is not filled in yet
    if (weather_alert_flag && (alert->index == -1) ) {

        // For weather alerts:
        // Need to figure out from the initial characters of the filename which
        // type of file we're using, then compute the fields we're looking for.
        // After we know that, need to look in the DBF file for a match.  Once
        // we find a match, we can open up the SHX/SHP files, go straight to
        // the shape we want, and draw it.
        switch (filenm[0]) {

            case 'c':   // County File
                // County, c_ files:  WI_C037
                // STATE  CWA  COUNTYNAME
                // AL     BMX  Morgan
                // Need fields 0/1:
                search_field1 = 0;  // STATE
                search_field2 = 3;  // FIPS
                xastir_snprintf(search_param1,sizeof(search_param1),"%c%c",
                    alert->title[0],
                    alert->title[1]);
                xastir_snprintf(search_param2,sizeof(search_param2),"%c%c%c",
                    alert->title[4],
                    alert->title[5],
                    alert->title[6]);
                break;

            case 'w':   // County Warning Area File
                // County Warning Area, w_ files:  CW_ATAE
                // WFO  CWA
                // TAE  TAE
                // Need field 0
                search_field1 = 0;  // WFO
                search_field2 = -1;
                xastir_snprintf(search_param1,sizeof(search_param1),"%c%c%c",
                    alert->title[4],
                    alert->title[5],
                    alert->title[6]);
                break;

            case 'o':   // Offshore Marine Area File
                // Offshore Marine Zones, oz files:  AN_Z081
                // ID      WFO  NAME
                // ANZ081  MPC  Gulf of Maine
                // Need field 0
                search_field1 = 0;  // ID
                search_field2 = -1;
                xastir_snprintf(search_param1,sizeof(search_param1),"%c%c%c%c%c%c",
                    alert->title[0],
                    alert->title[1],
                    alert->title[3],
                    alert->title[4],
                    alert->title[5],
                    alert->title[6]);
                break;

            case 'm':   // Marine Area File
                // Marine Zones, mz?????? files:  PK_Z120
                // ID      WFO  NAME
                // PKZ120  AJK  Area 1B. Southeast Alaska,
                // Need field 0
                search_field1 = 0;  // ID
                search_field2 = -1;
                xastir_snprintf(search_param1,sizeof(search_param1),"%c%c%c%c%c%c",
                    alert->title[0],
                    alert->title[1],
                    alert->title[3],
                    alert->title[4],
                    alert->title[5],
                    alert->title[6]);
                break;

            case 'z':   // Zone File
            default:
                // Weather alert zones, z_ files:  KS_Z033
                // STATE_ZONE
                // AK225
                // Need field 4
                search_field1 = 4;  // STATE_ZONE
                search_field2 = -1;
                xastir_snprintf(search_param1,sizeof(search_param1),"%c%c%c%c%c",
                    alert->title[0],
                    alert->title[1],
                    alert->title[4],
                    alert->title[5],
                   alert->title[6]);
                break;
        }

        //fprintf(stderr,"Search_param1: %s,\t",search_param1);
        //fprintf(stderr,"Search_param2: %s\n",search_param2);
    }

    for (i=0; i < fieldcount; i++) {
        char szTitle[12];

        switch (DBFGetFieldInfo(hDBF, i, szTitle, &nWidth, &nDecimals)) {
        case FTString:
            strcpy(ftype, "string");;
            break;

        case FTInteger:
            strcpy(ftype, "integer");
            break;

        case FTDouble:
            strcpy(ftype, "float");
            break;

        case FTInvalid:
            strcpy(ftype, "invalid/unsupported");
            break;

        default:
            strcpy(ftype, "unknown");
            break;
        }

        // Check for quad overlay type of map
        if (strstr(filename,"24kgrid")) {  // USGS Quad overlay file
            quad_overlay_flag++;
        }

        // Check the filename for mapshots.com filetypes to see what
        // type of file we may be dealing with.
        if (strncasecmp(filename,"tgr",3) == 0) {   // Found Mapshots or GeographyNetwork file

            if (strstr(filename,"lpt")) {           // Point file
                mapshots_labels_flag++;
                if (debug_level & 16) {
                    fprintf(stderr,"*** Found point file ***\n");
                    break;
                }
                else
                    break;
            }
            else if (strstr(filename,"plc")) {         // Designated Places:  Arlington
                city_flag++;
                mapshots_labels_flag++;
                if (debug_level & 16) {
                    fprintf(stderr,"*** Found (Designated Places) ***\n");
                    break;
                }
                else
                    break;
            }
            else if (strstr(filename,"ctycu")) {    // County Boundaries: WA, Snohomish
                if (debug_level & 16) {
                    fprintf(stderr,"*** Found county (mapshots county) ***\n");
                    break;
                }
                else
                    break;
            }
            else if (strstr(filename,"lkA")) {      // Roads
                road_flag++;
                mapshots_labels_flag++;
                if (debug_level & 16) {
                    fprintf(stderr,"*** Found some roads (mapshots roads) ***\n");
                    break;
                }
                else
                    break;
            }
            else if (strstr(filename,"lkB")) {      // Railroads
                railroad_flag++;
                mapshots_labels_flag++;
                if (debug_level & 16) {
                    fprintf(stderr,"*** Found some railroads (mapshots railroads) ***\n");
                    break;
                }
                else
                    break;
            }
            else if (strstr(filename,"lkC")) {      // Paths/etc.  Pipelines?  Transmission lines?
                path_flag++;
                if (debug_level & 16) {
                    fprintf(stderr,"*** Found some paths (mapshots paths/etc) ***\n");
                    break;
                }
                else
                    break;
            }
            else if (strstr(filename,"lkH")) {      // Rivers/Streams/Lakes/Glaciers
                river_flag++;
                mapshots_labels_flag++;
                if (debug_level & 16) {
                    fprintf(stderr,"*** Found water (mapshots rivers/streams/lakes/glaciers) ***\n");
                    break;
                }
                else
                    break;
            }
            else if (strstr(filename,"urb")) {      // Urban areas: Seattle, WA
                if (debug_level & 16) {
                    fprintf(stderr,"*** Found (mapshots urban areas) ***\n");
                    break;
                }
                else
                    break;
            }
            else if (strstr(filename,"wat")) {      // Bodies of water, creeks/lakes/glaciers
                lake_flag++;
                mapshots_labels_flag++;
                if (debug_level & 16) {
                    fprintf(stderr,"*** Found some water (mapshots bodies of water, creeks/lakes/glaciers) ***\n");
                    break;
                }
                else
                    break;
            }
        }


        // Attempt to guess which type of shapefile we're dealing
        // with, and how we should draw it.
        // If debug is on, we want to print out every field, otherwise
        // break once we've made our guess on the type of shapefile.
        if (debug_level & 16)
            fprintf(stderr,"%15.15s\t%15s  (%d,%d)\n", szTitle, ftype, nWidth, nDecimals);

        if (strncasecmp(szTitle, "SPEEDLIM", 8) == 0) {
            // sewroads shapefile?
            road_flag++;
            if (debug_level & 16)
                fprintf(stderr,"*** Found some roads (SPEEDLIM*) ***\n");
            else
                break;
        }
        else if (strncasecmp(szTitle, "US_RIVS_ID", 10) == 0) {
            // which shapefile?
            river_flag++;
            if (debug_level & 16)
                fprintf(stderr,"*** Found some rivers (US_RIVS_ID*) ***\n");
            else
                break;
        }
        else if (strcasecmp(szTitle, "FEATURE") == 0) {
            char *attr_str;
            int j;
            for (j=0; j < recordcount; j++) {
                if (fieldcount >= (i+1)) {
                    attr_str = (char*)DBFReadStringAttribute(hDBF, j, i);
                    if (strncasecmp(attr_str, "LAKE", 4) == 0) {
                        // NOAA Lakes and Water Bodies (lk17de98) shapefile
                        lake_flag++;
                        if (debug_level & 16)
                            fprintf(stderr,"*** Found some lakes (FEATURE == LAKE*) ***\n");
                        break;
                    }
                    else if (strstr(attr_str, "Highway") != NULL ||
                             strstr(attr_str, "highway") != NULL ||
                             strstr(attr_str, "HIGHWAY") != NULL) {
                         // NOAA Interstate Highways of the US (in011502) shapefile
                         // NOAA Major Roads of the US (rd011802) shapefile
                         road_flag++;
                         if (debug_level & 16)
                             fprintf(stderr,"*** Found some roads (FEATURE == *HIGHWAY*) ***\n");
                         break;
                     }
                }
            }
            if (!(debug_level & 16) && (lake_flag || road_flag))
                break;
        }
        else if (strcasecmp(szTitle, "LENGTH") == 0 ||
                 strcasecmp(szTitle, "RR")     == 0 ||
                 strcasecmp(szTitle, "HUC")    == 0 ||
                 strcasecmp(szTitle, "TYPE")   == 0 ||
                 strcasecmp(szTitle, "SEGL")   == 0 ||
                 strcasecmp(szTitle, "PMILE")  == 0 ||
                 strcasecmp(szTitle, "ARBSUM") == 0 ||
                 strcasecmp(szTitle, "PNAME")  == 0 ||
                 strcasecmp(szTitle, "OWNAME") == 0 ||
                 strcasecmp(szTitle, "PNMCD")  == 0 ||
                 strcasecmp(szTitle, "OWNMCD") == 0 ||
                 strcasecmp(szTitle, "DSRR")   == 0 ||
                 strcasecmp(szTitle, "DSHUC")  == 0 ||
                 strcasecmp(szTitle, "USDIR")  == 0) {
            // NOAA Rivers of the US (rv14fe02) shapefile
            // NOAA Rivers of the US Subset (rt14fe02) shapefile
            river_flag++;
            if (river_flag >= 14) {
                if (debug_level & 16)
                    fprintf(stderr,"*** Found some rivers (NOAA Rivers of the US or Subset) ***\n");
                else
                    break;
            }
        }
    }


    // Search for specific record if we're doing alerts
    if (weather_alert_flag && (alert->index == -1) ) {
        int done = 0;
        char *string1;
        char *string2;

        // Step through all records
        for( i = 0; i < recordcount && !done; i++ ) {
            char *ptr;
            switch (filenm[0]) {
                case 'c':   // County File
                    // Remember that there's only one place for
                    // internal storage of the DBF string.  This is
                    // why this code is organized with two "if"
                    // statements.
                    if (fieldcount >= (search_field1 + 1) ) {
                        string1 = (char *)DBFReadStringAttribute(hDBF,i,search_field1);
                        if (!strncasecmp(search_param1,string1,2)) {
                            //fprintf(stderr,"Found state\n");
                            if (fieldcount >= (search_field2 + 1) ) {
                                string2 = (char *)DBFReadStringAttribute(hDBF,i,search_field2);
                                ptr = string2;
                                ptr += 2;   // Skip past first two characters of FIPS code
                                if (!strncasecmp(search_param2,ptr,3)) {
//fprintf(stderr,"Found it!  %s\tShape: %d\n",string1,i);
                                    done++;
                                    found_shape = i;
                                }
                            }
                        }
                    }
                    break;
                case 'w':   // County Warning Area File
                    if (fieldcount >= (search_field1 + 1) ) {
                        string1 = (char *)DBFReadStringAttribute(hDBF,i,search_field1);
                        if ( !strncasecmp(search_param1,string1,strlen(string1))
                                && (strlen(string1) != 0) ) {
//fprintf(stderr,"Found it!  %s\tShape: %d\n",string1,i);
                            done++;
                            found_shape = i;
                        }
                    }
                    break;
                case 'o':   // Offshore Marine Area File
                    if (fieldcount >= (search_field1 + 1) ) {
                        string1 = (char *)DBFReadStringAttribute(hDBF,i,search_field1);
                        if ( !strncasecmp(search_param1,string1,strlen(string1))
                                && (strlen(string1) != 0) ) {
//fprintf(stderr,"Found it!  %s\tShape: %d\n",string1,i);
                            done++;
                            found_shape = i;
                        }
                    }
                    break;
                case 'm':   // Marine Area File
                    if (fieldcount >= (search_field1 + 1) ) {
                        string1 = (char *)DBFReadStringAttribute(hDBF,i,search_field1);
                        if ( !strncasecmp(search_param1,string1,strlen(string1))
                                && (strlen(string1) != 0) ) {
//fprintf(stderr,"Found it!  %s\tShape: %d\n",string1,i);
                            done++;
                            found_shape = i;
                        }
                    }
                    break;
                case 'z':   // Zone File
                    if (fieldcount >= (search_field1 + 1) ) {
                        string1 = (char *)DBFReadStringAttribute(hDBF,i,search_field1);
                        if ( !strncasecmp(search_param1,string1,strlen(string1))
                                && (strlen(string1) != 0) ) {
//fprintf(stderr,"Found it!  %s\tShape: %d\n",string1,i);
                            done++;
                            found_shape = i;
                        }
                    }
                default:
                    break;
            }
        }
        alert->index = found_shape; // Fill it in 'cuz we just found it
    }
    else if (weather_alert_flag) {
        // We've been here before and we already know the index into the
        // file to fetch this particular shape.
        found_shape = alert->index;
    }

    //fprintf(stderr,"Found shape: %d\n", found_shape);

    if (debug_level & 16)
        fprintf(stderr,"Calling SHPOpen()\n");

    // Open the .shx/.shp files for reading.
    // These are the index and the vertice files.
    hSHP = SHPOpen( file, "rb" );
    if( hSHP == NULL ) {
        fprintf(stderr,"draw_shapefile_map: SHPOpen(%s,\"rb\") failed.\n", file );
        DBFClose( hDBF );   // Clean up open file descriptors

        // Free up any malloc's that we did
        if (panWidth)
            free(panWidth);

        return;
    }

    // Get the extents of the map file
    SHPGetInfo( hSHP, &nEntities, &nShapeType, adfBndsMin, adfBndsMax );


    // Check whether we're indexing or drawing the map
    if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
            || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {

        xastir_snprintf(status_text, sizeof(status_text), langcode ("BBARSTA039"), filenm);
        statusline(status_text,0);       // Indexing ...

        // We're indexing only.  Save the extents in the index.
        index_update_ll(filenm,	// Filename only
            adfBndsMin[1],  // Bottom
            adfBndsMax[1],  // Top
            adfBndsMin[0],  // Left
            adfBndsMax[0]); // Right

        DBFClose( hDBF );   // Clean up open file descriptors
        SHPClose( hSHP );

        // Free up any malloc's that we did
        if (panWidth)
            free(panWidth);

        return; // Done indexing this file
    }
    else {
        xastir_snprintf(status_text, sizeof(status_text), langcode ("BBARSTA028"), filenm);
        statusline(status_text,0);       // Loading ...
    }
 

    switch ( nShapeType ) {
        case SHPT_POINT:
            strcpy(sType,"Point");
            break;

        case SHPT_ARC:
            strcpy(sType,"Polyline");
            break;

        case SHPT_POLYGON:
            strcpy(sType,"Polygon");
            break;

        case SHPT_MULTIPOINT:
            fprintf(stderr,"Multi-Point Shapefile format not implemented: %s\n",file);
            strcpy(sType,"MultiPoint");
            DBFClose( hDBF );   // Clean up open file descriptors
            SHPClose( hSHP );

            // Free up any malloc's that we did
            if (panWidth)
                free(panWidth);

            return; // Multipoint type.  Not implemented yet.
            break;

        default:
            DBFClose( hDBF );   // Clean up open file descriptors
            SHPClose( hSHP );

            // Free up any malloc's that we did
            if (panWidth)
                free(panWidth);

            return; // Unknown type.  Don't know how to process it.
            break;
    }

    if (debug_level & 16)
        fprintf(stderr,"%s(%d), %d Records in file\n",sType,nShapeType,nEntities);

    if (debug_level & 16)
        fprintf(stderr,"File Bounds: (%15.10g,%15.10g)\n\t(%15.10g,%15.10g)\n",
            adfBndsMin[0], adfBndsMin[1], adfBndsMax[0], adfBndsMax[1] );

    // Check the bounding box for this shapefile.  If none of the
    // file is within our viewport, we can skip the entire file.

    if (debug_level & 16)
        fprintf(stderr,"Calling map_visible_lat_lon on the entire shapefile\n");

    if (! map_visible_lat_lon(  adfBndsMin[1],  // Bottom
                                adfBndsMax[1],  // Top
                                adfBndsMin[0],  // Left
                                adfBndsMax[0],  // Right
                                file) ) {       // Error text if failure
        if (debug_level & 16)
            fprintf(stderr,"No shapes within viewport.  Skipping file...\n");

        DBFClose( hDBF );   // Clean up open file descriptors
        SHPClose( hSHP );

        // Free up any malloc's that we did
        if (panWidth)
            free(panWidth);

        return;     // The file contains no shapes in our viewport
    }


    // Update the statusline for this map name
    // Check whether we're indexing or drawing the map
    if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
            || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {
        xastir_snprintf(status_text, sizeof(status_text), langcode ("BBARSTA039"), filenm);
    }
    else {
        xastir_snprintf(status_text, sizeof(status_text), langcode ("BBARSTA028"), filenm);
    }
    statusline(status_text,0);       // Loading/Indexing ...


    // Set a default line width for all maps.  This will most likely
    // be modified for particular maps in later code.
    (void)XSetLineAttributes(XtDisplay(w), gc, 0, LineSolid, CapButt,JoinMiter);


    // NOTE: Setting the color here and in the "else" may not stick if we do more
    //       complex drawing further down like a SteelBlue lake with a black boundary,
    //       or if we have labels turned on which resets our color to black.
    if (weather_alert_flag) {
        char xbm_path[MAX_FILENAME];
        int _w, _h, _xh, _yh;
        int ret_val;

        // This GC is used for weather alerts (writing to the
        // pixmap: pixmap_alerts) and _was_ used for beam_heading
        // rays, but no longer is.
        (void)XSetForeground (XtDisplay (w), gc_tint, colors[(int)alert_color]);

        // N7TAP: No more tinting as that would change the color of the alert, losing that information.
        (void)XSetFunction(XtDisplay(w), gc_tint, GXcopy);
        /*
        Options are:
            GXclear         0                       (Don't use)
            GXand           src AND dst             (Darker colors, black can result from overlap)
            GXandReverse    src AND (NOT dst)       (Darker colors)
            GXcopy          src                     (Don't use)
            GXandInverted   (NOT src) AND dst       (Pretty colors)
            GXnoop          dst                     (Don't use)
            GXxor           src XOR dst             (Don't use, overlapping areas cancel each other out)
            GXor            src OR dst              (More pastel colors, too bright?)
            GXnor           (NOT src) AND (NOT dst) (Darker colors, very readable)
            GXequiv         (NOT src) XOR dst       (Bright, very readable)
            GXinvert        (NOT dst)               (Don't use)
            GXorReverse     src OR (NOT dst)        (Bright, not as readable as others)
            GXcopyInverted  (NOT src)               (Don't use)
            GXorInverted    (NOT src) OR dst        (Bright, not very readable)
            GXnand          (NOT src) OR (NOT dst)  (Bright, not very readable)
            GXset           1                       (Don't use)
        */

        if (strncasecmp(alert->alert_tag, "FLOOD", 5) == 0)
            xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "flood.xbm");
        else if (strncasecmp(alert->alert_tag, "SNOW", 4) == 0)
            xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "snow.xbm");
        else if (strncasecmp(alert->alert_tag, "TORNDO", 6) == 0)
            xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "tornado.xbm");
        else if (strncasecmp(alert->alert_tag, "WIND", 4) == 0)
            xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "wind.xbm");
        else if (strncasecmp(alert->alert_tag, "WINTER_STORM", 12) == 0)
            xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "wntr_strm.xbm");
        else if (strncasecmp(alert->alert_tag, "WINTER_WEATHER", 14) == 0)
            xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "winter_wx.xbm");
        else
            xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "alert.xbm");

        (void)XSetLineAttributes(XtDisplay(w), gc_tint, 0, LineSolid, CapButt,JoinMiter);
        XFreePixmap(XtDisplay(w), pixmap_wx_stipple);
        ret_val = XReadBitmapFile(XtDisplay(w), DefaultRootWindow(XtDisplay(w)),
                        xbm_path, &_w, &_h, &pixmap_wx_stipple, &_xh, &_yh);

        if (ret_val != 0) {
            fprintf(stderr,"Bitmap not found: %s\n",xbm_path);
            exit(1);
        }

        (void)XSetStipple(XtDisplay(w), gc_tint, pixmap_wx_stipple);
    }
    else {
// Are these actually used anymore by the code?  Colors get set later
// when we know more about what we're dealing with.
        if (lake_flag || river_flag)
            (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x1a]); // Steel Blue
        else if (path_flag)
            (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]); // black
        else if (railroad_flag)
            (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x01]); // purple
        else if (city_flag)
            (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x0e]); // yellow
        else
            (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]); // black
    }


    // Now that we have the file open, we can read out the structures.
    // We can handle Point, PolyLine and Polygon shapefiles at the moment.

    if (weather_alert_flag) {   // We're drawing _one_ weather alert shape
        if (found_shape != -1) {    // Found the record
            start_record = found_shape;
            end_record = found_shape + 1;
        }
        else {  // Didn't find the record
            start_record = 0;
            end_record = 0;
        }
    }
    else {  // Draw an entire Shapefile map
        start_record = 0;
        end_record = nEntities;
    }


    // Here's where we actually iterate through the entire file, drawing
    // each structure as we find it.
    for (structure = start_record; structure < end_record; structure++) {
        int skip_it = 0;
        int skip_label = 0;

        // Have had segfaults before at the SHPReadObject() call
        // when the Shapefile was corrupted.
        //fprintf(stderr,"Before SHPReadObject:%d\n",structure);

        object = SHPReadObject( hSHP, structure );  // Note that each structure can have multiple rings

        //fprintf(stderr,"After SHPReadObject\n");

        if (object == NULL)
            continue;   // Skip this iteration, go on to the next

        // Fill in the boundary variables in the alert record.  We use
        // this info in load_alert_maps() to determine which alerts are
        // within our view, without having to open up the shapefiles to
        // get this info (faster).
        if (weather_alert_flag) {
            alert->top_boundary    = object->dfYMax;
            alert->left_boundary   = object->dfXMin;
            alert->bottom_boundary = object->dfYMin;
            alert->right_boundary  = object->dfXMax;
        }

        // Here we check the bounding box for this shape against our
        // current viewport.  If we can't see it, don't draw it.

        if (debug_level & 16)
            fprintf(stderr,"Calling map_visible_lat_lon on a shape\n");

        // Set up some warning text just in case the map has a
        // problem.
        xastir_snprintf(warning_text,
            sizeof(warning_text),
            "File:%s, Structure:%d",
            file,
            structure);

        //fprintf(stderr,"%s\n",warning_text);

        if ( map_visible_lat_lon( object->dfYMin,   // Bottom
                                  object->dfYMax,   // Top
                                  object->dfXMin,   // Left
                                  object->dfXMax,   // Right
                                  warning_text) ) { // Error text if failure

            const char *temp;
            char temp2[100];
            int jj;
            int x0 = 0; // Used for computing label rotation
            int x1 = 0;
            int y0 = 0;
            int y1 = 0;


            if (debug_level & 16) {
                fprintf(stderr,"Shape %d is visible, drawing it.", structure);
                fprintf(stderr,"  Parts in shape: %d\n", object->nParts );    // Number of parts in this structure
            }

            if (alert)
                alert->flags[0] = 'Y';


            if (debug_level & 16) {
                // Print the field contents
                for (jj = 0; jj < fieldcount; jj++) {
                    if (fieldcount >= (jj + 1) ) {
                        temp = DBFReadStringAttribute( hDBF, structure, jj );
                        if (temp != NULL) {
                            fprintf(stderr,"%s, ", temp);
                        }
                    }
                }
                fprintf(stderr,"\n");
                fprintf(stderr,"Done with field contents\n");
            }


            switch ( nShapeType ) {


                case SHPT_POINT:
                    // We hit this case once for each point shape in
                    // the file, iff that shape is within our
                    // viewport.


                    if (debug_level & 16)
                        fprintf(stderr,"Found Point Shapefile\n");

                    // Read each point, place a label there, and an optional symbol
                    //object->padfX
                    //object->padfY
                    //object->padfZ
 
//                    if (    mapshots_labels_flag
//                            && map_labels
//                            && (fieldcount >= 3) ) {

                    if (1) {    // Need a bracket so we can define
                                // some local variables.
                        const char *temp = NULL;
                        int ok = 1;
                        int temp_ok;

                        // If labels are enabled and we have enough
                        // fields in the .dbf file, read the label.
                        if (map_labels && fieldcount >= 1) {
                            // Snag the label from the .dbf file
                            temp = DBFReadStringAttribute( hDBF, structure, 0 );
                        }

                        // Convert point to Xastir coordinates
                        temp_ok = convert_to_xastir_coordinates(&my_long,
                            &my_lat,
                            (float)object->padfX[0],
                            (float)object->padfY[0]);
                        //fprintf(stderr,"%ld %ld\n", my_long, my_lat);

                        if (!temp_ok) {
                            fprintf(stderr,"draw_shapefile_map: Problem converting from lat/lon\n");
                            ok = 0;
                            x = 0;
                            y = 0;
                        }
                        else {
                            // Convert to screen coordinates.  Careful
                            // here!  The format conversions you'll need
                            // if you try to compress this into two
                            // lines will get you into trouble.
                            x = my_long - x_long_offset;
                            y = my_lat - y_lat_offset;
                            x = x / scale_x;
                            y = y / scale_y;

                            if (x >  16000) ok = 0;     // Skip this point
                            if (x < -16000) ok = 0;     // Skip this point
                            if (y >  16000) ok = 0;     // Skip this point
                            if (y < -16000) ok = 0;     // Skip this point
                        }

                        if (ok == 1) {
                            char symbol_table = '/';
                            char symbol_id = '.'; /* small x */
                            char symbol_over = ' ';

                            // Fine-tuned the location here so that
                            // the middle of the 'X' would be at the
                            // proper pixel.
                            symbol(w, 0, symbol_table, symbol_id, symbol_over, pixmap, 1, x-10, y-10, ' ');

                            // Fine-tuned this string so that it is
                            // to the right of the 'X' and aligned
                            // nicely.
                            if (map_labels) {
                                draw_nice_string(w, pixmap, 0, x+10, y+5, (char*)temp, 0xf, 0x10, strlen(temp));
                                //(void)draw_label_text ( w, x, y, strlen(temp), colors[0x08], (char *)temp);
                                //(void)draw_rotated_label_text (w, 90, x+10, y, strlen(temp), colors[0x08], (char *)temp);
                            }
                        }
                    }
                    break;



                case SHPT_ARC:
                    // We hit this case once for each polyline shape
                    // in the file, iff at least part of that shape
                    // is within our viewport.

                    if (debug_level & 16)
                        fprintf(stderr,"Found Polylines\n");

// Draw the PolyLines themselves:

                    // Default in case we forget to set the line
                    // width later:
                    (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineSolid, CapButt,JoinMiter);
 
                    index = 0;  // Index into our own points array.
                                // Tells how many points we've
                                // collected so far.
 
                    // Read the vertices for each line
                    for (ring = 0; ring < object->nVertices; ring++ ) {
                        int temp_ok;

                        ok = 1;

                        //fprintf(stderr,"\t%d:%g %g\t", ring, object->padfX[ring], object->padfY[ring] );
                        // Convert to Xastir coordinates
                        temp_ok = convert_to_xastir_coordinates(&my_long,
                            &my_lat,
                            (float)object->padfX[ring],
                            (float)object->padfY[ring]);
                        //fprintf(stderr,"%ld %ld\n", my_long, my_lat);

                        if (!temp_ok) {
                            fprintf(stderr,"draw_shapefile_map: Problem converting from lat/lon\n");
                            ok = 0;
                            x = 0;
                            y = 0;
                        }
                        else {
                            // Convert to screen coordinates.  Careful
                            // here!  The format conversions you'll need
                            // if you try to compress this into two
                            // lines will get you into trouble.
                            x = my_long - x_long_offset;
                            y = my_lat - y_lat_offset;
                            x = x / scale_x;
                            y = y / scale_y;


                            // Save the endpoints of the first line
                            // segment for later use in label rotation
                            if (ring == 0) {
                                // Save the first set of screen coordinates
                                x0 = (int)x;
                                y0 = (int)y;
                            }
                            else if (ring == 1) {
                                // Save the second set of screen coordinates
                                x1 = (int)x;
                                y1 = (int)y;
                            }
    
                            // XDrawLines uses 16-bit unsigned integers
                            // (shorts).  Make sure we stay within the
                            // limits.
                            if (x >  16000) ok = 0;     // Skip this point
                            if (x < -16000) ok = 0;     // Skip this point
                            if (y >  16000) ok = 0;     // Skip this point
                            if (y < -16000) ok = 0;     // Skip this point
                        }
 
                        if (ok == 1) {
                            points[index].x = (short)x;
                            points[index].y = (short)y;
                            //fprintf(stderr,"%d %d\t", points[index].x, points[index].y);
                            index++;
                        }
                        if (index > high_water_mark_index)
                            high_water_mark_index = index;

                        if (index >= MAX_MAP_POINTS) {
                            index = MAX_MAP_POINTS - 1;
                            fprintf(stderr,"Trying to overrun the points array: SHPT_ARC, index=%d\n",index);
                        }
                    }

// Set up width and zoom level for roads
                    if (road_flag) {
                        int lanes = 0;
                        int dashed_line = 0;

                        if ( mapshots_labels_flag && (fieldcount >= 9) ) {
                            const char *temp;

                            temp = DBFReadStringAttribute( hDBF, structure, 8 );    // CFCC Field
                            switch (temp[1]) {
                                case '1':   // A1? = Primary road or interstate highway
                                    lanes = 4;
                                    (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x04]); // brown
                                    break;
                                case '2':   // A2? = Primary road w/o limited access, US highways
                                    lanes = 3;
                                    (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]); // black
                                    break;
                                case '3':   // A3? = Secondary road & connecting road, state highways
                                    if (map_color_levels && scale_y > 256)
                                        skip_label++;
                                    lanes = 2;
                                    (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]); // black
                                    switch (temp[2]) {
                                        case '1':
                                        case '2':
                                        case '3':
                                        case '4':
                                        case '5':
                                        case '6':
                                            break;
                                        case '7':
                                        case '8':
                                        default:
                                            if (map_color_levels && scale_y > 128)
                                                skip_label++;
                                            break;
                                    }
                                    break;
                                case '4':   // A4? = Local, neighborhood & rural roads, city streets
                                    // Skip the road if we're above this zoom level
                                    if (map_color_levels && scale_y > 96)
                                        skip_it++;
                                    // Skip labels above this zoom level to keep things uncluttered
                                    if (map_color_levels && scale_y > 16)
                                        skip_label++;
                                    lanes = 1;
                                    (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x28]); // gray35
                                    break;
                                case '5':   // A5? = Vehicular trail passable only by 4WD vehicle
                                    // Skip the road if we're above this zoom level
                                    if (map_color_levels && scale_y > 64)
                                        skip_it++;
                                    if (map_color_levels && scale_y > 16)
                                        skip_label++;
                                    lanes = 1;
                                    dashed_line++;
                                    (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x28]); // gray35
                                    break;
                                case '6':   // A6? = Cul-de-sac, traffic circles, access ramp,
                                            // service drive, ferry crossing
                                    switch (temp[2]) {
                                        case '5':   // Ferry crossing
                                            lanes = 2;
                                            dashed_line++;
                                            (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]); // black
                                           break;
                                        default:
                                            lanes = 1;
                                            // Skip the road if we're above this zoom level
                                            if (map_color_levels && scale_y > 64)
                                                skip_it++;
                                            if (map_color_levels && scale_y > 16)
                                                skip_label++;
                                            (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x28]); // gray35
                                            break;
                                    }

                                    break;
                                case '7':   // A7? = Walkway or pedestrian trail, stairway,
                                            // alley, driveway or service road
                                    // Skip the road if we're above this zoom level
                                    if (map_color_levels && scale_y > 64)
                                        skip_it++;
                                    if (map_color_levels && scale_y > 16)
                                        skip_label++;
                                    lanes = 1;
                                    dashed_line++;

                                    switch (temp[2]) {
                                        case '1':   // Walkway or trail for pedestrians
                                        case '2':   // Stairway or stepped road for pedestrians
                                            (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x0c]); // red
                                            break;
                                        default:
                                            (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x28]); // gray35
                                            break;
                                    }

                                    break;
                                default:
                                    lanes = 1;
                                    (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x28]); // gray35
                                    break;
                            }
                        }
                        else {  // Must not be a mapshots or ESRI Tiger map
                            if (fieldcount >= 7) {  // Need at least 7 fields if we're snagging #6, else segfault
                                lanes = DBFReadIntegerAttribute( hDBF, structure, 6 );

                                // In case we guess wrong on the
                                // type of file and the lanes are
                                // really out of bounds:
                                if (lanes < 1 || lanes > 10) {
                                    //fprintf(stderr,"lanes = %d\n",lanes);
                                    lanes = 1;
                                }
                            }
                            (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x28]); // gray35
                        }

                        if (lanes != (int)NULL) {
                            if (dashed_line) {
                                (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineOnOffDash, CapButt,JoinMiter);
                            }
                            else {
                                (void)XSetLineAttributes (XtDisplay (w), gc, lanes, LineSolid, CapButt,JoinMiter);
                            }
                        }
                        else {
                            (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineSolid, CapButt,JoinMiter);
                        }
                    }

// Set up width and zoom levels for water
                    else if (river_flag || lake_flag) {
                        int lanes = 0;
                        int dashed_line = 0;
                        int glacier_flag = 0;

                        if ( mapshots_labels_flag && (fieldcount >= 9) ) {
                            const char *temp;

                            temp = DBFReadStringAttribute( hDBF, structure, 8 );    // CFCC Field
                            switch (temp[1]) {
                                case '0':   // H0? = Water feature/shoreline
                                    if (map_color_levels && scale_y > 16)
                                        skip_label++;
                                    lanes = 0;

                                    switch (temp[2]) {
                                        case '2':   // Intermittent
                                            dashed_line++;
                                           break;
                                        default:
                                            break;
                                    }

                                   break;
                                case '1':
                                    if (map_color_levels && scale_y > 128)
                                        skip_label++;
                                    switch (temp[2]) {
                                        case '0':
                                            if (map_color_levels && scale_y > 16)
                                                skip_label++;
                                            lanes = 1;
                                            break;
                                        case '1':
                                            if (map_color_levels && scale_y > 16)
                                                skip_label++;
                                            lanes = 1;
                                            break;
                                        case '2':
                                            if (map_color_levels && scale_y > 16)
                                                skip_label++;
                                            lanes = 1;
                                            dashed_line++;
                                            break;
                                        case '3':
                                            if (map_color_levels && scale_y > 16)
                                                skip_label++;
                                            lanes = 1;
                                            break;
                                        default:
                                            if (map_color_levels && scale_y > 16)
                                                skip_label++;
                                            lanes = 1;
                                            break;
                                    }
                                    break;
                                case '2':
                                    if (map_color_levels && scale_y > 16)
                                        skip_label++;
                                    lanes = 1;
 
                                    switch (temp[2]) {
                                        case '2':   // Intermittent
                                            dashed_line++;
                                           break;
                                        default:
                                            break;
                                    }

                                   break;
                                case '3':
                                    if (map_color_levels && scale_y > 16)
                                        skip_label++;
                                    lanes = 1;
 
                                    switch (temp[2]) {
                                        case '2':   // Intermittent
                                            dashed_line++;
                                           break;
                                        default:
                                            break;
                                    }

                                   break;
                                case '4':
                                    if (map_color_levels && scale_y > 16)
                                        skip_label++;
                                    lanes = 1;
 
                                    switch (temp[2]) {
                                        case '2':   // Intermittent
                                            dashed_line++;
                                           break;
                                        default:
                                            break;
                                    }

                                   break;
                                case '5':
                                    if (map_color_levels && scale_y > 16)
                                        skip_label++;
                                    lanes = 1;
                                    break;
                                case '6':
                                    if (map_color_levels && scale_y > 16)
                                        skip_label++;
                                    lanes = 1;
                                    break;
                                case '7':   // Nonvisible stuff.  Don't draw these
                                    skip_it++;
                                    skip_label++;
                                    break;
                                case '8':
                                    if (map_color_levels && scale_y > 16)
                                        skip_label++;
                                    lanes = 1;
  
                                    switch (temp[2]) {
                                        case '1':   // Glacier
                                            glacier_flag++;
                                           break;
                                        default:
                                            break;
                                    }

                                   break;
                                default:
                                    if (map_color_levels && scale_y > 16)
                                        skip_label++;
                                    lanes = 1;
                                    break;
                            }
                            if (dashed_line) {
                                (void)XSetLineAttributes (XtDisplay (w), gc, lanes, LineOnOffDash, CapButt,JoinMiter);
                            }
                            else {
                                (void)XSetLineAttributes (XtDisplay (w), gc, lanes, LineSolid, CapButt,JoinMiter);
                            }
                        }
                        else {  // We don't know how wide to make it, not a mapshots or ESRI Tiger maps
                            if (dashed_line) {
                                (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineOnOffDash, CapButt,JoinMiter);
                            }
                            else {
                                (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineSolid, CapButt,JoinMiter);
                            }
                        }
                        if (glacier_flag)
                            (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x0f]); // white
                        else
                            (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x1a]); // Steel Blue
                    }   // End of river_flag/lake_flag code

                    else {  // Set default line width, use whatever color is already defined by this point.
                        (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineSolid, CapButt,JoinMiter);
                    }


//WE7U
// I'd like to be able to change the color of each GPS track for
// each team in the field.  It'll help to differentiate the tracks
// where they happen to cross.

                    if (gps_flag) {
                        int jj;
                        int done = 0;


                        // Fill in the label we'll use later
                        xastir_snprintf(gps_label,
                            sizeof(gps_label),
                            filename);

                        // Knock off the "_Color.shp" portion of the
                        // label.  Find the last underline character
                        // and change it to an end-of-string.
                        jj = strlen(gps_label);
                        while ( !done && (jj > 0) ) {
                            if (gps_label[jj] == '_') {
                                gps_label[jj] = '\0';   // Terminate it here
                                done++;
                            }
                            jj--;
                        }


                        // Check for a color in the filename: i.e.
                        // "Team2_Track_Red.shp"
                        if (strstr(filenm,"_Red.shp")) {
                            gps_color = 0x0c; // Red
                        }
                        else if (strstr(filenm,"_Green.shp")) {
//                            gps_color = 0x64; // ForestGreen
                            gps_color = 0x23; // Area Green Hi
                        }
                        else if (strstr(filenm,"_Black.shp")) {
                            gps_color = 0x08; // black
                        }
                        else if (strstr(filenm,"_White.shp")) {
                            gps_color = 0x0f; // white
                        }
                        else if (strstr(filenm,"_Orange.shp")) {
//                            gps_color = 0x06; // orange
//                            gps_color = 0x19; // orange2
//                            gps_color = 0x41; // DarkOrange3 (good medium orange)
                            gps_color = 0x62; // orange3 (brighter)
                        }
                        else if (strstr(filenm,"_Blue.shp")) {
                            gps_color = 0x03; // cyan
                        }
                        else if (strstr(filenm,"_Yellow.shp")) {
                            gps_color = 0x0e; // yellow
                        }
                        else if (strstr(filenm,"_Purple.shp")) {
                            gps_color = 0x0b; // mediumorchid
                        }
                        else {  // Default color
                            gps_color = 0x0c; // Red
                        }

                        // Set the color for the arc's
                        (void)XSetForeground(XtDisplay(w), gc, colors[gps_color]);
 
                        // Make the track nice and wide: Easy to
                        // see.
//                        (void)XSetLineAttributes (XtDisplay (w), gc, 3, LineSolid, CapButt,JoinMiter);
                        (void)XSetLineAttributes (XtDisplay (w), gc, 3, LineOnOffDash, CapButt,JoinMiter);
//                        (void)XSetLineAttributes (XtDisplay (w), gc, 3, LineDoubleDash, CapButt,JoinMiter);
                    }


                    if (ok_to_draw && !skip_it) {
                        (void)XDrawLines(XtDisplay(w), pixmap, gc, points, index, CoordModeOrigin);
                    }


// Figure out and draw the labels for PolyLines.  Note that we later
// determine whether we want to draw the label at all.  Move all
// code possible below that decision point to keep everything fast.
// Don't do unnecessary calculations if we're not going to draw the
// label.

                    // We're done with drawing the arc's.  Draw the
                    // labels in this next section.
                    //
                    temp = "";
                    if (       !skip_label
                            && !skip_it
                            && map_labels
                            && road_flag) {
                            char a[2],b[2],c[2];

                        if ( (mapshots_labels_flag) && (fieldcount >= 8) ) {
                            char temp3[3];
                            char temp4[31];
                            char temp5[5];
                            char temp6[3];

                            temp = DBFReadStringAttribute( hDBF, structure, 4 );
                            xastir_snprintf(temp3,sizeof(temp3),"%s",temp);
                            temp = DBFReadStringAttribute( hDBF, structure, 5 );
                            xastir_snprintf(temp4,sizeof(temp4),"%s",temp);
                            temp = DBFReadStringAttribute( hDBF, structure, 6 );
                            xastir_snprintf(temp5,sizeof(temp5),"%s",temp);
                            temp = DBFReadStringAttribute( hDBF, structure, 7 );
                            xastir_snprintf(temp6,sizeof(temp6),"%s",temp);

                            // Take care to not insert spaces if
                            // some of the strings are empty.
                            if (strlen(temp3) != 0) strcpy(a," ");
                            else                    a[0] = '\0';
                            if (strlen(temp4) != 0) strcpy(b," ");
                            else                    b[0] = '\0';
                            if (strlen(temp5) != 0) strcpy(c," ");
                            else                    c[0] = '\0';

                            xastir_snprintf(temp2,sizeof(temp2),"%s%s%s%s%s%s%s",
                                temp3,a,temp4,b,temp5,c,temp6);
                            temp = temp2;
                        }
                        else if (fieldcount >=10) {  // Need at least 10 fields if we're snagging #9, else segfault
                            // For roads, we need to use SIGN1 if it exists, else use DESCRIP if it exists.
                            temp = DBFReadStringAttribute( hDBF, structure, 9 );    // SIGN1
                        }
                        if ( (temp == NULL) || (strlen(temp) == 0) ) {
                            if (fieldcount >=13)    // Need at least 13 fields if we're snagging #12, else segfault
                                temp = DBFReadStringAttribute( hDBF, structure, 12 );    // DESCRIP
                            else
                                temp = NULL;
                        }
                    }
                    else if (!skip_label
                            && map_labels
                            && !skip_it
                            && (lake_flag || river_flag) ) {

                        if ( mapshots_labels_flag && river_flag && (fieldcount >= 8) ) {
                            char temp3[3];
                            char temp4[31];
                            char temp5[5];
                            char temp6[3];


                            temp = DBFReadStringAttribute( hDBF, structure, 4 );
                            xastir_snprintf(temp3,sizeof(temp3),"%s",temp);
                            temp = DBFReadStringAttribute( hDBF, structure, 5 );
                            xastir_snprintf(temp4,sizeof(temp4),"%s",temp);
                            temp = DBFReadStringAttribute( hDBF, structure, 6 );
                            xastir_snprintf(temp5,sizeof(temp5),"%s",temp);
                            temp = DBFReadStringAttribute( hDBF, structure, 7 );
                            xastir_snprintf(temp6,sizeof(temp6),"%s",temp);
                            xastir_snprintf(temp2,sizeof(temp2),"%s %s %s %s",
                                temp3,temp4,temp5,temp6);
                            temp = temp2;
                        }
                        else if (mapshots_labels_flag && lake_flag && (fieldcount >= 4) ) {
                            temp = DBFReadStringAttribute( hDBF, structure, 3 );
                        }
                        else if (fieldcount >=14) {  // Need at least 14 fields if we're snagging #13, else segfault
                            temp = DBFReadStringAttribute( hDBF, structure, 13 );   // PNAME (rivers)
                        }
                        else
                            temp = NULL;
                    }


                    // First we need to convert over to using the
                    // temp2 variable, which is changeable.  Make
                    // temp point to it.  temp may already be
                    // pointing to the temp2 variable.

                    if (temp != temp2) {
                        // temp points to an unchangeable string

                        if (temp != NULL) { // NOAA interstates file has a NULL at this point
                            strncpy(temp2,temp,sizeof(temp2));  // Copy the string so we can change it
                            temp = temp2;                       // Point temp to it (for later use)
                        }
                        else {
                            temp2[0] = '\0';
                        }
                    }
                    else {  // We're already set to work on temp2!
                    }


                    if ( map_labels && gps_flag ) {
                        // We're drawing GPS info.  Use gps_label,
                        // overriding anything set before.
                        xastir_snprintf(temp2,
                            sizeof(temp2),
                            "%s",
                            gps_label);
                    }


                    // Change "United States Highway 2" into "US 2"
                    // Look for substring at start of string
                    if ( strstr(temp2,"United States Highway") == temp2 ) {
                        int index;
                        // Convert to "US"
                        temp2[1] = 'S';  // Add an 'S'
                        index = 2;
                        while (temp2[index+19] != '\0') {
                            temp2[index] = temp2[index+19];
                            index++;
                        }
                        temp2[index] = '\0';
                    }
                    else {  // Change "State Highway 204" into "State 204"
                        // Look for substring at start of string
                        if ( strstr(temp2,"State Highway") == temp2 ) {
                            int index;
                            // Convert to "State"
                            index = 5;
                            while (temp2[index+8] != '\0') {
                            temp2[index] = temp2[index+8];
                            index++;
                            }
                            temp2[index] = '\0';
                        }
                        else {  // Change "State Route 2" into "State 2"
                            // Look for substring at start of string
                            if ( strstr(temp2,"State Route") == temp2 ) {
                                int index;
                                // Convert to "State"
                                index = 5;
                                while (temp2[index+6] != '\0') {
                                temp2[index] = temp2[index+6];
                                index++;
                                }
                                temp2[index] = '\0';
                            }
                        }
                    }

                    if ( (temp != NULL)
                            && (strlen(temp) != 0)
                            && map_labels
                            && !skip_it
                            && !skip_label ) {
                        int temp_ok;

                        ok = 1;

                        // Convert to Xastir coordinates
                        temp_ok = convert_to_xastir_coordinates(&my_long,
                            &my_lat,
                            (float)object->padfX[0],
                            (float)object->padfY[0]);
                        //fprintf(stderr,"%ld %ld\n", my_long, my_lat);

                        if (!temp_ok) {
                            fprintf(stderr,"draw_shapefile_map: Problem converting from lat/lon\n");
                            ok = 0;
                            x = 0;
                            y = 0;
                        }
                        else {
                            // Convert to screen coordinates.  Careful
                            // here!  The format conversions you'll need
                            // if you try to compress this into two
                            // lines will get you into trouble.
                            x = my_long - x_long_offset;
                            y = my_lat - y_lat_offset;
                            x = x / scale_x;
                            y = y / scale_y;

                            if (x >  16000) ok = 0;     // Skip this point
                            if (x < -16000) ok = 0;     // Skip this point
                            if (y >  16000) ok = 0;     // Skip this point
                            if (y < -16000) ok = 0;     // Skip this point
                        }

                        if (ok == 1 && ok_to_draw) {
                            int new_label = 1;
                            int mod_number;

                            // Set up the mod_number, which is used
                            // below to determine how many of each
                            // identical label are skipped at each
                            // zoom level.
                            if      (scale_y <= 2)
                                mod_number = 1;
                            else if (scale_y <= 4)
                                mod_number = 3;
                            else if (scale_y <= 8)
                                mod_number = 5;
                            else if (scale_y <= 16)
                                mod_number = 10;
                            else if (scale_y <= 32)
                                mod_number = 15;
                            else
                                mod_number = 20;

                            // Check whether we've written out this string
                            // already:  Look for a match in our linked list

// The problem with this method is that we might get strings
// "written" at the extreme top or right edge of the display, which
// means the strings wouldn't be visible, but Xastir thinks that it
// wrote the string out visibly.  To partially counteract this I've
// set it up to write only some of the identical strings.  This
// still doesn't help in the cases where a street only comes in from
// the top or right and doesn't have an intersection with another
// street (and therefore another label) within the view.

                            ptr2 = label_ptr;
                            while (ptr2 != NULL) {   // Step through the list
                                if (strcasecmp(ptr2->label,temp) == 0) {    // Found a match
                                    //fprintf(stderr,"Found a match!\t%s\n",temp);
                                    new_label = 0;
                                    ptr2->found = ptr2->found + 1;  // Increment the "found" quantity

// We change this "mod" number based on zoom level, so that long
// strings don't overwrite each other, and so that we don't get too
// many or too few labels drawn.  This will cause us to skip
// intersections (the tiger files appear to have a label at each
// intersection).  Between rural and urban areas, this method might
// not work well.  Urban areas have few intersections, so we'll get
// fewer labels drawn.
// A better method might be to check the screen location for each
// one and only write the strings if they are far enough apart, and
// only count a string as written if the start of it is onscreen and
// the angle is correct for it to be written on the screen.

                                    // Draw a number of labels
                                    // appropriate for the zoom
                                    // level.
                                    if ( ((ptr2->found - 1) % mod_number) != 0 )
                                        skip_label++;
                                    ptr2 = NULL; // End the loop
                                }
                                else {
                                    ptr2 = ptr2->next;
                                }
                            }
                            if (!skip_label) {  // Draw the string

                                // Compute the label rotation angle
                                float diff_X = (int)x1 - x0;
                                float diff_Y = (int)y1 - y0;
                                float angle = 0.0;  // Angle for the beginning of this polyline
                        
                                if (diff_X == 0.0) {  // Avoid divide by zero errors
                                    diff_X = 0.0000001;
                                }
                                angle = atan( diff_X / diff_Y );    // Compute in radians
                                // Convert to degrees
                                angle = angle / (2.0 * M_PI );
                                angle = angle * 360.0;

                                // Change to fit our rotate label function's idea of angle
                                angle = 360.0 - angle;

                                //fprintf(stderr,"Y: %f\tX: %f\tAngle: %f ==> ",diff_Y,diff_X,angle);

                                if ( angle > 90.0 ) {angle += 180.0;}
                                if ( angle >= 360.0 ) {angle -= 360.0;}

                                //fprintf(stderr,"%f\t%s\n",angle,temp);

//                              (void)draw_label_text ( w, x, y, strlen(temp), colors[0x08], (char *)temp);
                                if (gps_flag) {
                                    (void)draw_rotated_label_text (w,
                                        //(int)angle,
                                        -90,    // Horizontal, easiest to read
                                        x,
                                        y,
                                        strlen(temp),
                                        colors[gps_color],
                                        (char *)temp);
                                }
                                else {
                                    (void)draw_rotated_label_text(w,
                                        (int)angle,
                                        x,
                                        y,
                                        strlen(temp),
                                        colors[0x08],
                                        (char *)temp);
                                }
                            }
                            if (new_label) {

                                // Create a new record for this string
                                // and add it to the head of the list.
                                // Make sure to "free" this linked
                                // list.
                                //fprintf(stderr,"Creating a new record: %s\n",temp);
                                ptr2 = (label_string *)malloc(sizeof(label_string));
                                xastir_snprintf(ptr2->label,sizeof(ptr2->label),"%s",temp);
                                ptr2->found = 1;
                                ptr2->next = label_ptr;
                                label_ptr = ptr2;
                                //if (label_ptr->next == NULL)
                                //    fprintf(stderr,"only one record\n");
                            }
                        }
                    }
                    break;



                case SHPT_POLYGON:

                    if (debug_level & 16)
                        fprintf(stderr,"Found Polygons\n");

                    // Each polygon can be made up of multiple
                    // rings, and each ring has multiple points that
                    // define it.  We hit this case once for each
                    // polygon shape in the file, iff at least part
                    // of that shape is within our viewport.

// We now handle the "hole" drawing in polygon shapefiles, where
// clockwise direction around the ring means a fill, and CCW means a
// hole in the polygon.
//
// Possible implementations:
//
// 1) Snag an algorithm for a polygon "fill" function from
// somewhere, but add a piece that will check for being inside a
// "hole" polygon and just not draw while traversing it (change the
// pen color to transparent over the holes?).
// SUMMARY: How to do this?
//
// 2) Draw to another layer, then copy only the filled pixels to
// their final destination pixmap.
// SUMMARY: Draw polygons once, then a copy operation.
//
// 3) Separate area:  Draw polygon.  Copy from other map layer into
// holes, then copy the result back.
// SUMMARY:  How to determine outline?
//
// 4) Use clip-masks to prevent drawing over the hole areas:  Draw
// to another 1-bit pixmap or region.  This area can be the size of
// the max shape extents.  Filled = 1.  Holes = 0.  Use that pixmap
// as a clip-mask to draw the polygons again onto the final pixmap.
// SUMMARY: We end up drawing the shape twice!
//
// MODIFICATION:  Draw a filled rectangle onto the map pixmap using
// the clip-mask to control where it actually gets drawn.
// SUMMARY: Only end up drawing the shape once.
//
// 5) Inverted clip-mask:  Draw just the holes to a separate pixmap:
// Create a pixmap filled with 1's (XFillRectangle & GXset).  Draw
// the holes and use GXinvert to draw zero's to the mask where the
// holes go.  Use this as a clip-mask to draw the filled areas of
// the polygon to the map pixmap.
// SUMMARY: Faster than methods 1-4?
//
// 6) Use Regions to do the same method as #5 but with more ease.
// Create a polygon Region, then create a Region for each hole and
// subtract the hole from the polygon Region.  Once we have a
// complete polygon + holes, use that as the clip-mask for drawing
// the real polygon.  Use XSetRegion() on the GC to set this up.  We
// might have to do offsets as well so that the region maps properly
// to our map pixmap when we draw the final polygon to it.
// SUMMARY:  Should be faster than methods 1-5.
//
// 7) Do method 6 but instead of drawing a polygon region, draw a
// rectangle region first, then knock holes in it.  Use that region
// as the clip-mask for the XFillPolygon() later by calling
// XSetRegion() on the GC.  We don't really need a polygon region
// for a clip-mask.  A rectangle with holes in it will work just as
// well and should be faster overall.  We might have to do offsets
// as well so that the region maps properly to our map pixmap when
// we draw the final polygon to it.
// SUMMARY:  This might be the fastest method of the ones listed, if
// drawing a rectangle region is faster than drawing a polygon
// region.  We draw the polygon once here instead of twice, and each
// hole only once.  The only added drawing time would be the
// creation of the rectangle region, which should be fairly fast,
// and the subtracting of the hole regions from it.
//
//
// Shapefiles also allow identical points to be next to each other
// in the vertice list.  We should look for that and get rid of
// duplicate vertices.


//if (object->nParts > 1)
//fprintf(stderr,"Number of parts: %d\n", object->nParts);


// Unfortunately, for Polygon shapes we must make one pass through
// the entire set of rings to see if we have any "hole" rings (as
// opposed to "fill" rings).  If we have any "hole" rings, we must
// handle the drawing of the Shape quite differently.
//
// Read the vertices for each ring in the Shape.  Test whether we
// have a hole ring.  If so, save the ring index away for the next
// step when we actually draw the shape.

                    polygon_hole_flag = 0;

                    // Allocate storage for a flag for each ring in
                    // this Shape.
                    // !!Remember to free this storage later!!
                    polygon_hole_storage = (int *)malloc(object->nParts*sizeof(int));

// Run through the entire shape (all rings of it) once.  Create an
// array of flags that specify whether each ring is a fill or a
// hole.  If any holes found, set the global polygon_hole_flag as
// well.

                    for (ring = 0; ring < object->nParts; ring++ ) {

                        // Testing for fill or hole ring.  This will
                        // determine how we ultimately draw the
                        // entire shape.
                        //
                        switch ( shape_ring_direction( object, ring) ) {
                            case  0:    // Error in trying to compute whether fill or hole
                                fprintf(stderr,"Error in computing fill/hole ring\n");
                            case  1:    // It's a fill ring
                                // Do nothing for these two cases
                                // except clear the flag in our
                                // storage
                                polygon_hole_storage[ring] = 0;
                                break;
                            case -1:    // It's a hole ring
                                // Add it to our list of hole rings
                                // here and set a flag.  That way we
                                // won't have to run through
                                // SHPRingDir_2d again in the next
                                // loop.
                                polygon_hole_flag++;
                                polygon_hole_storage[ring] = 1;
//                                fprintf(stderr, "Ring %d/%d is a polygon hole\n",
//                                    ring,
//                                    object->nParts);
                                break;
                        }
                    }
// We're done with the initial run through the vertices of all
// rings.  We now know which rings are fills and which are holes and
// have recorded the data.



//WE7U3
                    // Speedup:  If not draw_filled, then don't go
                    // through the math and region code.  Set the
                    // flag to zero so that we won't do all the math
                    // and the regions.
                    if (!map_color_fill || !draw_filled)
                        polygon_hole_flag = 0;

                    if (polygon_hole_flag) {
                        XRectangle rectangle;
                        long width, height;
                        double top_ll, left_ll, bottom_ll, right_ll;
                        int temp_ok;


//                        fprintf(stderr, "%s:Found %d hole rings in shape %d\n",
//                            file,
//                            polygon_hole_flag,
//                            structure);

//WE7U3
////////////////////////////////////////////////////////////////////////

// Now that we know which are fill/hole rings, worry about drawing
// each ring of the Shape:
//
// 1) Create a filled rectangle region, probably the size of the
// Shape extents, and at the same screen coordinates as the entire
// shape would normally be drawn.
//
// 2) Create a region for each hole ring and subtract these new
// regions one at a time from the rectangle region created above.
//
// 3) When the "swiss-cheese" rectangle region is complete, draw
// only the filled polygons onto the map pixmap using the
// swiss-cheese rectangle region as the clip-mask.  Use a temporary
// GC for this operation, as I can't find a way to remove a
// clip-mask from a GC.  We may have to use offsets to make this
// work properly.


                        // Create three regions and rotate between
                        // them, due to the XSubtractRegion()
                        // needing three parameters.  If we later
                        // find that two of the parameters can be
                        // repeated, we can simplify our code.
                        // We'll rotate through them mod 3.

                        temp_region1 = 0;

                        // Create empty region
                        region[temp_region1] = XCreateRegion();

                        // Draw a rectangular clip-mask inside the
                        // Region.  Use the same extents as the full
                        // Shape.

                        // Set up the real sizes from the Shape
                        // extents.
                        top_ll    = object->dfYMax;
                        left_ll   = object->dfXMin;
                        bottom_ll = object->dfYMin;
                        right_ll  = object->dfXMax;

                        // Convert point to Xastir coordinates:
                        temp_ok = convert_to_xastir_coordinates(&my_long,
                            &my_lat,
                            left_ll,
                            top_ll);
                        //fprintf(stderr,"%ld %ld\n", my_long, my_lat);

                        if (!temp_ok) {
                            fprintf(stderr,"draw_shapefile_map: Problem converting from lat/lon\n");
                            ok = 0;
                            x = 0;
                            y = 0;
                        }
                        else {
                            // Convert to screen coordinates.  Careful
                            // here!  The format conversions you'll need
                            // if you try to compress this into two
                            // lines will get you into trouble.
                            x = my_long - x_long_offset;
                            y = my_lat - y_lat_offset;
                            x = x / scale_x;
                            y = y / scale_y;

                            // Here we check for really wacko points that will cause problems
                            // with the X drawing routines, and fix them.
                            if (x > 1700l) x = 1700l;
                            if (x <    0l) x =    0l;
                            if (y > 1700l) y = 1700l;
                            if (y <    0l) y =    0l;
                        }

                        // Convert points to Xastir coordinates
                        temp_ok = convert_to_xastir_coordinates(&my_long,
                            &my_lat,
                            right_ll,
                            bottom_ll);
                         //fprintf(stderr,"%ld %ld\n", my_long, my_lat);

                        if (!temp_ok) {
                            fprintf(stderr,"draw_shapefile_map: Problem converting from lat/lon\n");
                            ok = 0;
                            width = 0;
                            height = 0;
                        }
                        else {
                            // Convert to screen coordinates.  Careful
                            // here!  The format conversions you'll need
                            // if you try to compress this into two
                            // lines will get you into trouble.
                            width = my_long - x_long_offset;
                            height = my_lat - y_lat_offset;
                            width = width / scale_x;
                            height = height / scale_y;

                            // Here we check for really wacko points that will cause problems
                            // with the X drawing routines, and fix them.
                            if (width  > 1700l) width  = 1700l;
                            if (width  <    1l) width  =    1l;
                            if (height > 1700l) height = 1700l;
                            if (height <    1l) height =    1l;
                        }

//TODO
// We can run into trouble here because we only have 16-bit values
// to work with.  If we're zoomed in and the Shape is large in
// comparison to the screen, we'll easily exceed these numbers.
// Perhaps we'll need to work with something other than screen
// coordinates?  Perhaps truncating the values will be adequate.

                        rectangle.x      = (short) x;
                        rectangle.y      = (short) y;
                        rectangle.width  = (unsigned short) width;
                        rectangle.height = (unsigned short) height;

//fprintf(stderr,"*** Rectangle: %d,%d %dx%d\n", rectangle.x, rectangle.y, rectangle.width, rectangle.height);

                        // Create the initial region containing a
                        // filled rectangle.
                        XUnionRectWithRegion(&rectangle,
                            region[temp_region1],
                            region[temp_region1]);
 
                        // Create a region for each set of hole
                        // vertices (CCW rotation of the vertices)
                        // and subtract each from the rectangle
                        // region.
                        for (ring = 0; ring < object->nParts; ring++ ) {
                            int endpoint;
                            int on_screen;
 

                            if (polygon_hole_storage[ring] == 1) {
                                // It's a hole polygon.  Cut the
                                // hole out of our rectangle region.
                                int num_vertices = 0;
                                int nVertStart;

 
                                nVertStart = object->panPartStart[ring];

                                if( ring == object->nParts-1 )
                                    num_vertices = object->nVertices
                                                 - object->panPartStart[ring];
                                else
                                    num_vertices = object->panPartStart[ring+1] 
                                                 - object->panPartStart[ring];

//TODO
// Snag the vertices and put them into the "points" array,
// converting to screen coordinates as we go, then subtracting the
// starting point, so that the regions remain small?
//
// We could either subtract the starting point of each shape from
// each point, or take the hit on region size and just use the full
// screen size (or whatever part of it the shape required plus the
// area from the starting point to 0,0).


                                if ( (ring+1) < object->nParts)
                                    endpoint = object->panPartStart[ring+1];
                                    //else endpoint = object->nVertices;
                                else
                                    endpoint = object->panPartStart[0] + object->nVertices;
                                //fprintf(stderr,"Endpoint %d\n", endpoint);
                                //fprintf(stderr,"Vertices: %d\n", endpoint - object->panPartStart[ring]);

                                i = 0;  // i = Number of points to draw for one ring
                                on_screen = 0;
                                // index = ptr into the shapefile's array of points
                                for (index = object->panPartStart[ring]; index < endpoint; ) {
                                    int temp_ok;
       
 
                                    // Get vertice and convert to Xastir coordinates
                                    temp_ok = convert_to_xastir_coordinates(&my_long,
                                        &my_lat,
                                        (float)object->padfX[index],
                                        (float)object->padfY[index]);

                                    //fprintf(stderr,"%lu %lu\t", my_long, my_lat);

                                    if (!temp_ok) {
                                        fprintf(stderr,"draw_shapefile_map: Problem converting from lat/lon\n");
                                        ok = 0;
                                        x = 0;
                                        y = 0;
                                    }
                                    else {
                                        // Convert to screen coordinates.  Careful
                                        // here!  The format conversions you'll need
                                        // if you try to compress this into two
                                        // lines will get you into trouble.
                                        x = my_long - x_long_offset;
                                        y = my_lat - y_lat_offset;
                                        x = x / scale_x;
                                        y = y / scale_y;
        
                                        //fprintf(stderr,"%ld %ld\t\t", x, y);

                                        // Here we check for really
                                        // wacko points that will
                                        // cause problems with the X
                                        // drawing routines, and fix
                                        // them.  Increment
                                        // on_screen if any of the
                                        // points might be on
                                        // screen.
                                        if (x >  1700l)
                                            x =  1700l;
                                        else if (x < 0l)
                                            x = 0l;
                                        else on_screen++;

                                        if (y >  1700l)
                                            y =  1700l;
                                        else if (y < 0l)
                                            y = 0l;
                                        else on_screen++;

                                        points[i].x = (short)x;
                                        points[i].y = (short)y;

if (on_screen) {
//    fprintf(stderr,"%d x:%d y:%d\n",
//        i,
//        points[i].x,
//        points[i].y);
}
                                        i++;
                                    }
                                    index++;

                                    if (index > high_water_mark_index)
                                        high_water_mark_index = index;

                                    if (index > endpoint) {
                                        index = endpoint;
                                        fprintf(stderr,"Trying to run past the end of shapefile array: index=%d\n",index);
                                    }
                                }   // End of converting vertices for a ring


                                // Create and subtract the region
                                // only if it might be on screen.
                                if (on_screen) {
                                    temp_region2 = (temp_region1 + 1) % 3;
                                    temp_region3 = (temp_region1 + 2) % 3;

                                    // Create empty regions.
                                    region[temp_region2] = XCreateRegion();
                                    region[temp_region3] = XCreateRegion();

                                    // Draw the hole polygon
                                    if (num_vertices >= 3) {
                                        region[temp_region2] = XPolygonRegion(points,
                                            num_vertices,
                                            WindingRule);
                                    }
                                    else {
                                        fprintf(stderr,
                                            "draw_shapefile_map:XPolygonRegion with too few vertices:%d\n",
                                            num_vertices);
                                    }

                                    // Subtract region2 from region1 and
                                    // put the result into region3.
//fprintf(stderr, "Subtracting region\n");
                                    XSubtractRegion(region[temp_region1],
                                        region[temp_region2],
                                        region[temp_region3]);

                                    // Get rid of the two regions we no
                                    // longer need
                                    XDestroyRegion(region[temp_region1]);
                                    XDestroyRegion(region[temp_region2]);

                                    // Indicate the final result region for
                                    // the next iteration or the exit of the
                                    // loop.
                                    temp_region1 = temp_region3;
                                }
                            }
                        }

                        // region[temp_region1] now contains a
                        // clip-mask of the original polygon with
                        // holes cut out of it (swiss-cheese
                        // rectangle).

                        // Create temporary GC.  It looks like we
                        // don't need this to create the regions,
                        // but we'll need it when we draw the filled
                        // polygons onto the map pixmap using the
                        // final region as a clip-mask.

// Offsets?
// XOffsetRegion

//                        gc_temp_values.function = GXcopy;
                        gc_temp = XCreateGC(XtDisplay(w),
                            XtWindow(w),
                            0,
                            &gc_temp_values);

                        // Set the clip-mask into the GC.  This GC
                        // is now ruined for other purposes, so
                        // destroy it when we're done drawing this
                        // one shape.
                        XSetRegion(XtDisplay(w), gc_temp, region[temp_region1]);
                        XDestroyRegion(region[temp_region1]);
                    }
//WE7U3
////////////////////////////////////////////////////////////////////////


                    // Read the vertices for each ring in this Shape
                    for (ring = 0; ring < object->nParts; ring++ ) {
                        int endpoint;
                        int glacier_flag = 0;
                        const char *temp;


                        if (lake_flag || river_flag) {
                            if ( mapshots_labels_flag && (fieldcount >= 3) ) {
                                temp = DBFReadStringAttribute( hDBF, structure, 2 );    // CFCC Field
                                switch (temp[1]) {
                                    case '8':   // Special water feature
                                        switch(temp[2]) {
                                            case '1':
                                                glacier_flag++;  // Found a glacier
                                                break;
                                            default:
                                                break;
                                        }
                                        break;
                                    default:
                                        break;
                                }
                            }
                        }
 
                        //fprintf(stderr,"Ring: %d\t\t", ring);

                        if ( (ring+1) < object->nParts)
                            endpoint = object->panPartStart[ring+1];
                            //else endpoint = object->nVertices;
                        else
                            endpoint = object->panPartStart[0] + object->nVertices;

                        //fprintf(stderr,"Endpoint %d\n", endpoint);
                        //fprintf(stderr,"Vertices: %d\n", endpoint - object->panPartStart[ring]);

                        i = 0;  // i = Number of points to draw for one ring
                        // index = ptr into the shapefile's array of points
                        for (index = object->panPartStart[ring]; index < endpoint; ) {
                            int temp_ok;

                            ok = 1;

                            //fprintf(stderr,"\t%d:%g %g\t", index, object->padfX[index], object->padfY[index] );

                            // Get vertice and convert to Xastir coordinates
                            temp_ok = convert_to_xastir_coordinates(&my_long,
                                &my_lat,
                                (float)object->padfX[index],
                                (float)object->padfY[index]);

                            //fprintf(stderr,"%lu %lu\t", my_long, my_lat);

                            if (!temp_ok) {
                                fprintf(stderr,"draw_shapefile_map: Problem converting from lat/lon\n");
                                ok = 0;
                                x = 0;
                                y = 0;
                            }
                            else {
                                // Convert to screen coordinates.  Careful
                                // here!  The format conversions you'll need
                                // if you try to compress this into two
                                // lines will get you into trouble.
                                x = my_long - x_long_offset;
                                y = my_lat - y_lat_offset;
                                x = x / scale_x;
                                y = y / scale_y;

                                //fprintf(stderr,"%ld %ld\t\t", x, y);

                                // Here we check for really wacko points that will cause problems
                                // with the X drawing routines, and fix them.
                                if (x >  15000l) x =  15000l;
                                if (x < -15000l) x = -15000l;
                                if (y >  15000l) y =  15000l;
                                if (y < -15000l) y = -15000l;

                                points[i].x = (short)x;
                                points[i].y = (short)y;
                                i++;    // Number of points to draw

                                if (i > high_water_mark_i)
                                    high_water_mark_i = i;


                                if (i >= MAX_MAP_POINTS) {
                                    i = MAX_MAP_POINTS - 1;
                                    fprintf(stderr,"Trying to run past the end of our internal points array: i=%d\n",i);
                                }

                                //fprintf(stderr,"%d %d\t", points[i].x, points[i].y);

                                index++;

                                if (index > high_water_mark_index)
                                    high_water_mark_index = index;

                                if (index > endpoint) {
                                    index = endpoint;
                                    fprintf(stderr,"Trying to run past the end of shapefile array: index=%d\n",index);
                                }
                            }
                        }

                        if ( (i >= 3)
                                && (ok_to_draw)
                                && ( !draw_filled || !map_color_fill || (draw_filled && polygon_hole_storage[ring] == 0) ) ) {
                            // We have a polygon to draw!

//WE7U3
                            if ((!draw_filled || !map_color_fill) && polygon_hole_storage[ring] == 1) {
                                // We have a hole drawn as unfilled.
                                // Draw as a black dashed line.
                                (void)XSetForeground(XtDisplay(w), gc, colors[0x08]); // black for border
                                (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineOnOffDash, CapButt,JoinMiter);
                                (void)XDrawLines(XtDisplay(w), pixmap, gc, points, i, CoordModeOrigin);
                                (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineSolid, CapButt,JoinMiter);
                            }
                            else if (quad_overlay_flag) {
                                (void)XDrawLines(XtDisplay(w), pixmap, gc, points, i, CoordModeOrigin);
                            }
                            else if (glacier_flag) {
                                (void)XSetForeground(XtDisplay(w), gc, colors[0x0f]); // white
                                if (map_color_fill && draw_filled) {

                                    if (polygon_hole_flag) {
                                        (void)XSetForeground(XtDisplay(w), gc_temp, colors[0x0f]); // white

                                        if (i >= 3) {
                                            (void)XFillPolygon(XtDisplay(w),
                                                pixmap,
                                                gc_temp,
                                                points,
                                                i,
                                                Nonconvex,
                                                CoordModeOrigin);
                                        }
                                        else {
                                            fprintf(stderr,
                                                "draw_shapefile_map:Too few points:%d, Skipping XFillPolygon()",
                                                npoints);
                                        }
                                    }
                                    else {
                                        if (i >= 3) {
                                            (void)XFillPolygon(XtDisplay(w),
                                                pixmap,
                                                gc,
                                                points,
                                                i,
                                                Nonconvex,
                                                CoordModeOrigin);
                                        }
                                        else {
                                            fprintf(stderr,
                                                "draw_shapefile_map:Too few points:%d, Skipping XFillPolygon()",
                                                npoints);
                                        }
                                    }
                                }
                                (void)XDrawLines(XtDisplay(w), pixmap, gc, points, i, CoordModeOrigin);
                            }
                            else if (lake_flag) {
                                (void)XSetForeground(XtDisplay(w), gc, colors[0x1a]); // Steel Blue
                                if (map_color_fill && draw_filled) {

                                    if (polygon_hole_flag) {
                                        (void)XSetForeground(XtDisplay(w), gc_temp, colors[0x1a]); // Steel Blue

                                        if (i >= 3) {
                                            (void)XFillPolygon(XtDisplay(w),
                                                pixmap,
                                                gc_temp,
                                                points,
                                                i,
                                                Nonconvex,
                                                CoordModeOrigin);
                                        }
                                        else {
                                            fprintf(stderr,
                                                "draw_shapefile_map:Too few points:%d, Skipping XFillPolygon()",
                                                npoints);
                                        }
                                    }
                                    else {
                                        if (i >= 3) {
                                            (void)XFillPolygon(XtDisplay(w),
                                                pixmap,
                                                gc,
                                                points,
                                                i,
                                                Nonconvex,
                                                CoordModeOrigin);
                                        }
                                        else {
                                            fprintf(stderr,
                                                "draw_shapefile_map:Too few points:%d, Skipping XFillPolygon()",
                                                npoints);
                                        }
                                    }

//                                    (void)XSetForeground(XtDisplay(w), gc, colors[0x08]); // black for border
                                }
                                (void)XDrawLines(XtDisplay(w), pixmap, gc, points, i, CoordModeOrigin);
                            }
                            else if (river_flag) {
                                (void)XSetForeground(XtDisplay(w), gc, colors[0x1a]); // Steel Blue
                                if (map_color_fill && draw_filled) {

                                    if (polygon_hole_flag) {
                                        (void)XSetForeground(XtDisplay(w), gc_temp, colors[0x1a]); // Steel Blue

                                        if (i >= 3) {
                                            (void)XFillPolygon(XtDisplay(w),
                                                pixmap,
                                                gc_temp,
                                                points,
                                                i,
                                                Nonconvex,
                                                CoordModeOrigin);
                                        }
                                        else {
                                            fprintf(stderr,
                                                "draw_shapefile_map:Too few points:%d, Skipping XFillPolygon()",
                                                npoints);
                                        }
                                    }
                                    else {
                                        if (i >= 3) {
                                            (void)XFillPolygon(XtDisplay(w),
                                                pixmap,
                                                gc,
                                                points,
                                                i,
                                                Nonconvex,
                                                CoordModeOrigin);
                                        }
                                        else {
                                            fprintf(stderr,
                                                "draw_shapefile_map:Too few points:%d, Skipping XFillPolygon()",
                                                npoints);
                                        }
                                    }

                                }
                                else {
                                    (void)XDrawLines(XtDisplay(w), pixmap, gc, points, i, CoordModeOrigin);
                                }
                            }
                            else if (weather_alert_flag) {
                                (void)XSetFillStyle(XtDisplay(w), gc_tint, FillStippled);
// We skip the hole/fill thing for these?

                                if (i >= 3) {
                                    (void)XFillPolygon(XtDisplay(w),
                                        pixmap_alerts,
                                        gc_tint,
                                        points,
                                        i,
                                        Nonconvex,
                                        CoordModeOrigin);
                                }
                                else {
                                    fprintf(stderr,
                                        "draw_shapefile_map:Too few points:%d, Skipping XFillPolygon()",
                                        npoints);
                                }

                                (void)XSetFillStyle(XtDisplay(w), gc_tint, FillSolid);
                                (void)XDrawLines(XtDisplay(w), pixmap_alerts, gc_tint, points, i, CoordModeOrigin);
                            }
                            else if (map_color_fill && draw_filled) {  // Land masses?
                                if (polygon_hole_flag) {
                                    if (city_flag)
                                        (void)XSetForeground(XtDisplay(w), gc_temp, GetPixelByName(w,"RosyBrown"));  // RosyBrown, duh
                                    else
                                        (void)XSetForeground(XtDisplay(w), gc_temp, colors[0xff]); // grey

                                    if (i >= 3) {
                                        (void)XFillPolygon(XtDisplay(w),
                                            pixmap,
                                            gc_temp,
                                            points,
                                            i,
                                            Nonconvex,
                                            CoordModeOrigin);
                                    }
                                    else {
                                        fprintf(stderr,
                                            "draw_shapefile_map:Too few points:%d, Skipping XFillPolygon()",
                                            npoints);
                                    }
                                }
                                else {
                                    if (city_flag)
                                        (void)XSetForeground(XtDisplay(w), gc, GetPixelByName(w,"RosyBrown"));  // RosyBrown, duh
                                    else
                                        (void)XSetForeground(XtDisplay(w), gc, colors[0xff]); // grey

                                    if (i >= 3) {
                                        (void)XFillPolygon(XtDisplay (w),
                                            pixmap,
                                            gc,
                                            points,
                                            i,
                                            Nonconvex,
                                            CoordModeOrigin);
                                    }
                                    else {
                                        fprintf(stderr,
                                            "draw_shapefile_map:Too few points:%d, Skipping XFillPolygon()",
                                            npoints);
                                    }
                                }

                                (void)XSetForeground(XtDisplay(w), gc, colors[0x08]); // black for border

                                // Draw a thicker border for city boundaries
                                if (city_flag) {
                                    if (scale_y <= 64) {
                                        (void)XSetLineAttributes(XtDisplay(w), gc, 2, LineSolid, CapButt,JoinMiter);
                                    }
                                    else if (scale_y <= 128) {
                                        (void)XSetLineAttributes(XtDisplay(w), gc, 1, LineSolid, CapButt,JoinMiter);
                                    }
                                    else {
                                        (void)XSetLineAttributes(XtDisplay(w), gc, 0, LineSolid, CapButt,JoinMiter);
                                    }

                                    (void)XSetForeground(XtDisplay(w), gc, colors[0x14]); // lightgray for border
                                }
                                else {
                                    (void)XSetForeground(XtDisplay(w), gc, colors[0x08]); // black for border
                                }

                                (void)XDrawLines(XtDisplay(w), pixmap, gc, points, i, CoordModeOrigin);
                            }
                            else {  // Use whatever color is defined by this point.
                                (void)XSetLineAttributes(XtDisplay(w), gc, 0, LineSolid, CapButt,JoinMiter);
                                (void)XDrawLines(XtDisplay(w), pixmap, gc, points, i, CoordModeOrigin);
                            }
                        }
                    }

                    // Free the storage that we allocated to hold
                    // the "hole" flags for the shape.
                    free(polygon_hole_storage);

                    if (polygon_hole_flag) {
                        //Free the temporary GC that we may have used to
                        //draw polygons using the clip-mask:
                        XFreeGC(XtDisplay(w), gc_temp);
                    }


////////////////////////////////////////////////////////////////////////////////////////////////////
// Done with drawing shapes, now draw labels
////////////////////////////////////////////////////////////////////////////////////////////////////


                    temp = "";

                    if (lake_flag) {
                        if (map_color_levels && scale_y > 128)
                            skip_label++;
                        if (mapshots_labels_flag && (fieldcount >= 4) )
                            temp = DBFReadStringAttribute( hDBF, structure, 3 );
                        else if (fieldcount >= 1)
                            temp = DBFReadStringAttribute( hDBF, structure, 0 );    // NAME (lakes)
                        else
                            temp = NULL;
                    }
                    else if (city_flag) {
                        if (map_color_levels && scale_y > 512)
                            skip_label++;
                        if (mapshots_labels_flag && (fieldcount >= 4) )
                            temp = DBFReadStringAttribute( hDBF, structure, 3 );    // NAME (designated places)
                        else
                            temp = NULL;
                    }

                    if (quad_overlay_flag) {
                        if (fieldcount >= 5) {
                            // Use just the last two characters of
                            // the quad index.  "44072-A3" converts
                            // to "A3"
                            temp = DBFReadStringAttribute( hDBF, structure, 4 );
                            xastir_snprintf(quad_label,
                                sizeof(quad_label),
                                "%s ",
                                &temp[strlen(temp) - 2]);

                            // Append the name of the quad
                            temp = DBFReadStringAttribute( hDBF, structure, 0 );
                            strcat(quad_label,temp);
                        }
                        else {
                            quad_label[0] = '\0';
                        }
                    }

                    if ( (temp != NULL)
                            && (strlen(temp) != 0)
                            && map_labels
                            && !skip_label ) {
                        int temp_ok;

                        ok = 1;

                        // Convert to Xastir coordinates:
                        // If quad overlay shapefile, need to
                        // snag the label coordinates from the DBF
                        // file instead.  Note that the coordinates
                        // are for the bottom right corner of the
                        // quad, so we need to shift it left by 7.5'
                        // to make the label appear inside the quad
                        // (attached to the bottom left corner in
                        // this case).
                        if (quad_overlay_flag) {
                            const char *dbf_temp;
                            float lat_f;
                            float lon_f;

                            if (fieldcount >= 4) {
                                dbf_temp = DBFReadStringAttribute( hDBF, structure, 2 );
                                sscanf(dbf_temp, "%f", &lat_f);
                                dbf_temp = DBFReadStringAttribute( hDBF, structure, 3 );
                                sscanf(dbf_temp, "%f", &lon_f);
                                lon_f = lon_f - 0.125;
                            }
                            else {
                                lat_f = 0.0;
                                lon_f = 0.0;
                            }

                            //fprintf(stderr,"Lat: %f, Lon: %f\t, Quad: %s\n", lat_f, lon_f, quad_label);

                            temp_ok = convert_to_xastir_coordinates(&my_long,
                                &my_lat,
                                (float)lon_f,
                                (float)lat_f);
                        }
                        else {  // Not quad overlay, use vertices
                            temp_ok = convert_to_xastir_coordinates(&my_long,
                                &my_lat,
                                (float)object->padfX[0],
                                (float)object->padfY[0]);
                        }
                        //fprintf(stderr,"%ld %ld\n", my_long, my_lat);

                        if (!temp_ok) {
                            fprintf(stderr,"draw_shapefile_map: Problem converting from lat/lon\n");
                            ok = 0;
                            x = 0;
                            y = 0;
                        }
                        else {
                            // Convert to screen coordinates.  Careful
                            // here!  The format conversions you'll need
                            // if you try to compress this into two
                            // lines will get you into trouble.
                            x = my_long - x_long_offset;
                            y = my_lat - y_lat_offset;
                            x = x / scale_x;
                            y = y / scale_y;

                            if (x >  16000) ok = 0;     // Skip this point
                            if (x < -16000) ok = 0;     // Skip this point
                            if (y >  16000) ok = 0;     // Skip this point
                            if (y < -16000) ok = 0;     // Skip this point

                            if (ok == 1 && ok_to_draw) {
                                if (quad_overlay_flag) {
                                    draw_nice_string(w,
                                        pixmap,
                                        0,
                                        x+2,
                                        y-1,
                                        (char*)quad_label,
                                        0xf,
                                        0x10,
                                        strlen(quad_label));
                                }
                                else {
                                    (void)draw_label_text ( w,
                                        x,
                                        y,
                                        strlen(temp),
                                        colors[0x08],
                                        (char *)temp);
                                }
                            }
                        }
                    }
                    break;

                case SHPT_MULTIPOINT:
                        // Not implemented.
                        fprintf(stderr,"Shapefile Multi-Point format files aren't supported!\n");
                    break;

                default:
                        // Not implemented.
                        fprintf(stderr,"Shapefile format not supported: Subformat unknown (default clause of switch)!\n");
                    break;

            }   // End of switch
        }
        else {  // Shape not currently visible
            if (alert)
                alert->flags[0] = 'N';
        }
        SHPDestroyObject( object ); // Done with this structure
    }


    // Free our linked list of strings, if any
    ptr2 = label_ptr;
    while (ptr2 != NULL) {
        label_ptr = ptr2->next;
        //fprintf(stderr,"free: %s\n",ptr2->label);
        free(ptr2);
        ptr2 = label_ptr;
    }


    DBFClose( hDBF );
    SHPClose( hSHP );

    // Free up any malloc's that we did
    if (panWidth)
        free(panWidth);
 
//    XmUpdateDisplay (XtParent (da));

    if (debug_level & 16) {
        fprintf(stderr,"High-Mark Index:%d,\tHigh-Mark i:%d\n",
            high_water_mark_index,
            high_water_mark_i);
    }
}
// End of draw_shapefile_map()

#endif  // HAVE_LIBSHP





void Print_properties_destroy_shell(/*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);

begin_critical_section(&print_properties_dialog_lock, "maps.c:Print_properties_destroy_shell" );

    XtDestroyWidget(shell);
    print_properties_dialog = (Widget)NULL;

end_critical_section(&print_properties_dialog_lock, "maps.c:Print_properties_destroy_shell" );

}





// Print_window:  Prints the drawing area to a Postscript file.
//
void Print_window( Widget widget, XtPointer clientData, XtPointer callData ) {

#ifdef NO_GRAPHICS
    fprintf(stderr,"XPM or ImageMagick support not compiled into Xastir!\n");
#else   // NO_GRAPHICS

#ifndef HAVE_GV
    fprintf(stderr,"GV support not compiled into Xastir!\n");
#else   // HAVE_GV


   char xpm_filename[MAX_FILENAME];
    char ps_filename[MAX_FILENAME];
    char mono[50] = "";
    char invert[50] = "";
    char rotate[50] = "";
    char scale[50] = "";
    char density[50] = "";
    char command[MAX_FILENAME*2];
    char temp[100];
    char format[50] = "-portrait ";
    uid_t user_id;
    struct passwd *user_info;
    char username[20];


    // Get user info
    user_id=getuid();
    user_info=getpwuid(user_id);
    // Get my login name
    strcpy(username,user_info->pw_name);

    xastir_snprintf(xpm_filename, sizeof(xpm_filename), "/var/tmp/xastir_%s_print.xpm",
            username);
    xastir_snprintf(ps_filename, sizeof(ps_filename), "/var/tmp/xastir_%s_print.ps",
        username);

    busy_cursor(appshell);  // Show a busy cursor while we're doing all of this

    // Get rid of Print Properties dialog
    Print_properties_destroy_shell(widget, print_properties_dialog, NULL );


    if ( debug_level & 512 )
        fprintf(stderr,"Creating %s\n", xpm_filename );

    xastir_snprintf(temp, sizeof(temp), langcode("PRINT0012") );
    statusline(temp,1);       // Dumping image to file...

    if ( !XpmWriteFileFromPixmap(XtDisplay(appshell),  // Display *display
            xpm_filename,                               // char *filename
            pixmap_final,                               // Pixmap pixmap
            (Pixmap)NULL,                               // Pixmap shapemask
            NULL ) == XpmSuccess ) {                    // XpmAttributes *attributes
        fprintf(stderr,"ERROR writing %s\n", xpm_filename );
    }
    else {          // We now have the xpm file created on disk

        chmod( xpm_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

        if ( debug_level & 512 )
            fprintf(stderr,"Convert %s ==> %s\n", xpm_filename, ps_filename );


        // Convert it to a postscript file for printing.  This depends
        // on the ImageMagick command "convert".
        //
        // Other options to try in the future:
        // -label
        //
        if ( print_auto_scale ) {
//            sprintf(scale, "-geometry 612x792 -page 612x792 ");   // "Letter" size at 72 dpi
//            sprintf(scale, "-sample 612x792 -page 612x792 ");     // "Letter" size at 72 dpi
            xastir_snprintf(scale, sizeof(scale), "-page 1275x1650+0+0 "); // "Letter" size at 150 dpi
        }
        else
            scale[0] = '\0';    // Empty string


        if ( print_in_monochrome )
            xastir_snprintf(mono, sizeof(mono), "-monochrome +dither " );    // Monochrome
        else
            xastir_snprintf(mono, sizeof(mono), "+dither ");                // Color


        if ( print_invert )
            xastir_snprintf(invert, sizeof(invert), "-negate " );              // Reverse Colors
        else
            invert[0] = '\0';   // Empty string


        if (debug_level & 512)
            fprintf(stderr,"Width: %ld\tHeight: %ld\n", screen_width, screen_height);


        if ( print_rotated ) {
            xastir_snprintf(rotate, sizeof(rotate), "-rotate -90 " );
            xastir_snprintf(format, sizeof(format), "-landscape " );
        } else if ( print_auto_rotation ) {
            // Check whether the width or the height of the pixmap is greater.
            // If width is greater than height, rotate the image by 270 degrees.
            if (screen_width > screen_height) {
                xastir_snprintf(rotate, sizeof(rotate), "-rotate -90 " );
                xastir_snprintf(format, sizeof(format), "-landscape " );
                if (debug_level & 512)
                    fprintf(stderr,"Rotating\n");
            } else {
                rotate[0] = '\0';   // Empty string
                if (debug_level & 512)
                    fprintf(stderr,"Not Rotating\n");
            }
        } else {
            rotate[0] = '\0';   // Empty string
            if (debug_level & 512)
                fprintf(stderr,"Not Rotating\n");
        }


        // Higher print densities require more memory and time to process
        xastir_snprintf(density, sizeof(density), "-density %dx%d", print_resolution,
                print_resolution );

        xastir_snprintf(temp, sizeof(temp), langcode("PRINT0013") );
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

#ifdef HAVE_CONVERT
        xastir_snprintf(command,
            sizeof(command),
            "%s -filter Point %s%s%s%s%s %s %s",
            CONVERT_PATH,
            mono,
            invert,
            rotate,
            scale,
            density,
            xpm_filename,
            ps_filename );
        if ( debug_level & 512 )
            fprintf(stderr,"%s\n", command );

        if ( system( command ) != 0 ) {
            fprintf(stderr,"\n\nPrint: Couldn't convert from XPM to PS!\n\n\n");
            return;
        }
#endif  // HAVE_CONVERT

        chmod( ps_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

        // Delete temporary xpm file
        if ( !(debug_level & 512) )
            unlink( xpm_filename );

        if ( debug_level & 512 )
            fprintf(stderr,"Printing postscript file %s\n", ps_filename);

// Note: This needs to be changed to "lp" for Solaris.
// Also need to have a field to configure the printer name.  One
// fill-in field could do both.
//
// Since we could be running SUID root, we don't want to be
// calling "system" anyway.  Several problems with it.
/*
        xastir_snprintf(command,
            sizeof(command),
            "%s -Plp %s",
            LPR_PATH,
            ps_filename );
        if ( debug_level & 512 )
            fprintf(stderr,"%s\n", command);

        if ( system( command ) != 0 ) {
            fprintf(stderr,"\n\nPrint: Couldn't send to the printer!\n\n\n");
            return;
        }
*/


#ifdef HAVE_GV
        // Bring up the "gv" postscript viewer
        xastir_snprintf(command,
            sizeof(command),
//            "%s %s-scale -2 -media Letter %s &",
            "%s %s-scale -2 %s &",
            GV_PATH,
            format,
            ps_filename );

        if ( debug_level & 512 )
            fprintf(stderr,"%s\n", command);

        if ( system( command ) != 0 ) {
            fprintf(stderr,"\n\nPrint: Couldn't bring up the gv viewer!\n\n\n");
            return;
        }
#endif  // HAVE_GV

/*
        if ( !(debug_level & 512) )
            unlink( ps_filename );
*/

        if ( debug_level & 512 )
            fprintf(stderr,"  Done printing.\n");
    }

    xastir_snprintf(temp, sizeof(temp), langcode("PRINT0014") );
    statusline(temp,1);       // Finished creating print file.

    //popup_message( langcode("PRINT0015"), langcode("PRINT0014") );

#endif  // HAVE_GV
#endif // NO_GRAPHICS

}





/*
 *  Auto_rotate
 *
 */
void  Auto_rotate( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        print_auto_rotation = atoi(which);
        print_rotated = 0;
        XmToggleButtonSetState(rotate_90, FALSE, FALSE);
    } else {
        print_auto_rotation = 0;
    }
}





/*
 *  Rotate_90
 *
 */
void  Rotate_90( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        print_rotated = atoi(which);
        print_auto_rotation = 0;
        XmToggleButtonSetState(auto_rotate, FALSE, FALSE);
    } else {
        print_rotated = 0;
    }
}





/*
 *  Auto_scale
 *
 */
void  Auto_scale( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        print_auto_scale = atoi(which);
    } else {
        print_auto_scale = 0;
    }
}





/*
 *  Monochrome
 *
 */
void  Monochrome( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        print_in_monochrome = atoi(which);
    } else {
        print_in_monochrome = 0;
    }
}





/*
 *  Invert
 *
 */
void  Invert( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        print_invert = atoi(which);
    } else {
        print_invert = 0;
    }
}





// Print_properties:  Prints the drawing area to a PostScript file
//
// Perhaps later:
// 1) Select an area on the screen to print
// 2) -label
//
void Print_properties( Widget w, XtPointer clientData, XtPointer callData ) {
    static Widget pane, form, button_ok, button_cancel,
            sep, auto_scale,
//            paper_size, paper_size_data, scale, scale_data, blank_background,
//            res_label1, res_label2, res_x, res_y, button_preview,
            monochrome, invert;
    Atom delw;

    if (!print_properties_dialog) {


begin_critical_section(&print_properties_dialog_lock, "maps.c:Print_properties" );


        print_properties_dialog = XtVaCreatePopupShell(langcode("PRINT0001"),xmDialogShellWidgetClass,Global.top,
                                  XmNdeleteResponse,XmDESTROY,
                                  XmNdefaultPosition, FALSE,
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
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
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
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_FORM,
                                      XmNrightOffset, 10,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);
XtAddCallback(rotate_90,XmNvalueChangedCallback,Rotate_90,"1");


        auto_scale = XtVaCreateManagedWidget(langcode("PRINT0005"),xmToggleButtonWidgetClass,form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, auto_rotate,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
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
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);
XtAddCallback(monochrome,XmNvalueChangedCallback,Monochrome,"1");


        invert = XtVaCreateManagedWidget(langcode("PRINT0016"),xmToggleButtonWidgetClass,form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, monochrome,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
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
                                      NULL);


/*
        button_preview = XtVaCreateManagedWidget(langcode("PRINT0010"),xmPushButtonGadgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 0,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 1,
                                      XmNrightOffset, 2,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);
XtSetSensitive(button_preview,FALSE);
*/


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
                                      NULL);


//        XtAddCallback(button_preview, XmNactivateCallback, Print_window, "1" );
        XtAddCallback(button_ok, XmNactivateCallback, Print_window, "0" );
        XtAddCallback(button_cancel, XmNactivateCallback, Print_properties_destroy_shell, print_properties_dialog);


        XmToggleButtonSetState(rotate_90,FALSE,FALSE);
        XmToggleButtonSetState(auto_rotate,TRUE,FALSE);


        if (print_auto_rotation)
            XmToggleButtonSetState(auto_rotate, TRUE, TRUE);
        else
            XmToggleButtonSetState(auto_rotate, FALSE, TRUE);


        if (print_rotated)
            XmToggleButtonSetState(rotate_90, TRUE, TRUE);
        else
            XmToggleButtonSetState(rotate_90, FALSE, TRUE);


        if (print_in_monochrome)
            XmToggleButtonSetState(monochrome, TRUE, FALSE);
        else
            XmToggleButtonSetState(monochrome, FALSE, FALSE);


        if (print_invert)
            XmToggleButtonSetState(invert, TRUE, FALSE);
        else
            XmToggleButtonSetState(invert, FALSE, FALSE);


        if (print_auto_scale)
            XmToggleButtonSetState(auto_scale, TRUE, TRUE);
        else
            XmToggleButtonSetState(auto_scale, FALSE, TRUE);
 

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


    } else {
        (void)XRaiseWindow(XtDisplay(print_properties_dialog), XtWindow(print_properties_dialog));
    }
}





// Create png image (for use in web browsers??).  Requires that "convert"
// from the ImageMagick package be installed on the system.
//
static void* snapshot_thread(void *arg) {

#ifndef NO_GRAPHICS
    char xpm_filename[MAX_FILENAME];
    char png_filename[MAX_FILENAME];
#ifdef HAVE_CONVERT
    char command[MAX_FILENAME*2];
#endif  // HAVE_CONVERT
    uid_t user_id;
    struct passwd *user_info;
    char username[20];


    // The pthread_detach() call means we don't care about the
    // return code and won't use pthread_join() later.  Makes
    // threading more efficient.
    (void)pthread_detach(pthread_self());

    // Get user info 
    user_id=getuid();
    user_info=getpwuid(user_id);
    // Get my login name
    strcpy(username,user_info->pw_name);

    xastir_snprintf(xpm_filename, sizeof(xpm_filename), "/var/tmp/xastir_%s_snap.xpm",
            username);
    xastir_snprintf(png_filename, sizeof(png_filename),"/var/tmp/xastir_%s_snap.png",
            username);

    if ( debug_level & 512 )
        fprintf(stderr,"Creating %s\n", xpm_filename );

    if ( !XpmWriteFileFromPixmap( XtDisplay(appshell),    // Display *display
            xpm_filename,                               // char *filename
            pixmap_final,                               // Pixmap pixmap
            (Pixmap)NULL,                               // Pixmap shapemask
            NULL ) == XpmSuccess ) {                    // XpmAttributes *attributes
        fprintf(stderr,"ERROR writing %s\n", xpm_filename );
    }
    else {  // We now have the xpm file created on disk

        chmod( xpm_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

        if ( debug_level & 512 )
            fprintf(stderr,"Convert %s ==> %s\n", xpm_filename, png_filename );

#ifdef HAVE_CONVERT
        // Convert it to a png file.  This depends on having the
        // ImageMagick command "convert" installed.
        xastir_snprintf(command,
            sizeof(command),
            "%s -quality 100 -colors 256 %s %s",
            CONVERT_PATH,
            xpm_filename,
            png_filename );

        if ( system( command ) != 0 ) {
            // We _may_ have had an error.  Check errno to make
            // sure.
            if (errno) {
                fprintf(stderr, "%s\n", strerror(errno));
                fprintf(stderr,
                    "Failed to convert snapshot: %s -> %s\n",
                    xpm_filename,
                    png_filename);
            }
            else {
                fprintf(stderr,
                    "System call return error: convert: %s -> %s\n",
                    xpm_filename,
                    png_filename);
            }
        }
        else {
            chmod( png_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

            // Delete temporary xpm file
            unlink( xpm_filename );

            if ( debug_level & 512 )
                fprintf(stderr,"  Done creating png.\n");
        }
#endif  // HAVE_CONVERT
    }

#endif // NO_GRAPHICS
  
    // Signify that we're all done and that another snapshot can
    // occur.
    doing_snapshot = 0;

    return(NULL);
}





// Starts a separate thread that creates a png image from the
// current displayed image.
//
void Snapshot(void) {
    pthread_t snapshot_thread_id;


    // Check whether we're already doing a snapshot
    if (doing_snapshot)
        return;

    // Time to take another snapshot?
    if (sec_now() < (last_snapshot + 300) ) // New snapshot every five minutes
        return;

    doing_snapshot++;
    last_snapshot = sec_now(); // Set up timer for next time
 
//----- Start New Thread -----

    // Here we start a new thread.  We'll communicate with the main
    // thread via global variables.  Use mutex locks if there might
    // be a conflict as to when/how we're updating those variables.
    //

    if (pthread_create(&snapshot_thread_id, NULL, snapshot_thread, NULL)) {
        fprintf(stderr,"Error creating snapshot thread\n");
    }
    else {
        // We're off and running with the new thread!
    }
}





// Function to remove double-quote characters and spaces that occur
// outside of the double-quote characters.
void clean_string(char *input) {
    char *i;
    char *j;


    //fprintf(stderr,"|%s|\t",input);

    // Remove any double quote characters
    i = index(input,'"');   // Find first quote character, if any

    if (i != NULL) {
        j = index(i+1,'"'); // Find second quote character, if any

        if (j != NULL) {    // Found two quote characters
            j[0] = '\0';    // Terminate the string at the 2nd quote
            strcpy(input,i+1);
        }
        else {  // We only found one quote character.  What to do?
//            fprintf(stderr,"clean_string: Only one quote found!\n");
        }
    }
    //fprintf(stderr,"|%s|\n",input);

    // Remove leading/trailing spaces?
}





//NOTE:  This function has a problem if a non-gnis file is labeled
//with a ".gnis" extension.  It causes a segfault in Xastir.  More
//error checking needs to be done in order to prevent this.

// draw_gnis_map()
//
// Allows drawing a background map of labels for the viewport.
// Example format:
// "WA","Abbey View Memorial Park","cemetery","Snohomish","53","061","474647N","1221650W","47.77972","-122.28056","","","","","420","","Edmonds East"
//
// These types of files are available from http://geonames.usgs.gov/
// under "Download State Gazetteer Data - Available Via Anonymous FTP".
// A typical filename would be: "WA.deci.gz".   Do not get the other
// types of files which are columnar.  The files that we parse are
// comma-delimited and have quotes around each field.

// Some of the files have quotes around some fields, but not others,
// and some have an extreme amount of spaces at the end of each
// line.  Some also have spaces, after a field but before a comma.
//
// Another difference just found:  Some files have quotes around all
// fields, some have quotes around only some.  We need to handle
// this gracefully.
//
void draw_gnis_map (Widget w, char *dir, char *filenm, int destination_pixmap)
{
    char file[MAX_FILENAME];        // Complete path/name of GNIS file
    FILE *f;                        // Filehandle of GNIS file
    char line[MAX_FILENAME];        // One line of text from file
    char *i, *j, *k;
    char state[50];
    char name[200];
    char type[100];
    char county[100];
    char latitude[15];
    char longitude[15];
    char population[15];
    char lat_dd[3];
    char lat_mm[3];
    char lat_ss[3];
    char lat_dir[2];
    char long_dd[4];
    char long_mm[3];
    char long_ss[3];
    char long_dir[2];
    char lat_str[15];
    char long_str[15];
    int temp1;
    long coord_lon, coord_lat;
    long min_lat, min_lon, max_lat, max_lon;
    int ok;
    long x,y;
    char symbol_table, symbol_id, symbol_over;
    unsigned long bottom_extent = 0l;
    unsigned long top_extent = 0l;
    unsigned long left_extent = 0l;
    unsigned long right_extent = 0l;
    char status_text[MAX_FILENAME];


    //fprintf(stderr,"draw_gnis_map starting: %s/%s\n",dir,filenm);

    xastir_snprintf(file, sizeof(file), "%s/%s", dir, filenm);

    // Screen view
    min_lat = y_lat_offset + (long)(screen_height * scale_y);
    max_lat = y_lat_offset;
    min_lon = x_long_offset;
    max_lon = x_long_offset + (long)(screen_width  * scale_x);


    // The map extents in the map index are checked in draw_map to
    // see whether we should draw the map at all.


    // Update the statusline for this map name
    // Check whether we're indexing or drawing the map
    if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
            || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {
        xastir_snprintf(status_text, sizeof(status_text), langcode ("BBARSTA039"), filenm);
    }
    else {
        xastir_snprintf(status_text, sizeof(status_text), langcode ("BBARSTA028"), filenm);
    }
    statusline(status_text,0);       // Loading/Indexing ...


    // Attempt to open the file
    f = fopen (file, "r");
    if (f != NULL) {
        while (!feof (f)) {     // Loop through entire file

            if ( get_line (f, line, MAX_FILENAME) ) {  // Snag one line of data
                if (strlen(line) > 0) {

                    //NOTE:  We handle running off the end of "line"
                    //via the "continue" statement.  Skip the line
                    //if we don't find enough parameters while
                    //parsing.

                    // Find State feature resides in
                    i = index(line,',');    // Find ',' after state field

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    i[0] = '\0';
                    strncpy(state,line,49);
                    clean_string(state);

//NOTE:  It'd be nice to take the part after the comma and put it before the rest
// of the text someday, i.e. "Cassidy, Lake".

                    // Find Name
                    j = index(i+1, ',');    // Find ',' after Name.  Note that there may be commas in the name.

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    while ( (j != NULL) && (j[1] != '\"') ) {
                        k = j;
                        j = index(k+1, ',');
                    }

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    j[0] = '\0';
                    strncpy(name,i+1,199);
                    clean_string(name);

                    // Find Type
                    i = index(j+1, ',');

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    i[0] = '\0';
                    strncpy(type,j+1,99);
                    clean_string(type);

                    // Find County          // Can there be commas in the county name?
                    j = index(i+1, ',');

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    j[0] = '\0';
                    strncpy(county,i+1,99);
                    clean_string(county);

                    // Find ?
                    i = index(j+1, ',');

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    i[0] = '\0';

                    // Find ?
                    j = index(i+1, ',');

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    j[0] = '\0';

                    // Find latitude (DDMMSSN)
                    i = index(j+1, ',');

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    i[0] = '\0';
                    strncpy(latitude,j+1,14);
                    clean_string(latitude);

                    // Find longitude (DDDMMSSW)
                    j = index(i+1, ',');

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    j[0] = '\0';
                    strncpy(longitude,i+1,14);
                    clean_string(longitude);

                    // Find another latitude
                    i = index(j+1, ',');

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    i[0] = '\0';

                    // Find another longitude
                    j = index(i+1, ',');

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    j[0] = '\0';

                    // Find ?
                    i = index(j+1, ',');

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    i[0] = '\0';

                    // Find ?
                    j = index(i+1, ',');

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    j[0] = '\0';

                    // Find ?
                    i = index(j+1, ',');

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    i[0] = '\0';

                    // Find ?
                    j = index(i+1, ',');

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    j[0] = '\0';

                    // Find altitude
                    i = index(j+1, ',');

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    i[0] = '\0';

                    // Find population
                    j = index(i+1, ',');

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip this line
                    }

                    if ( j != NULL ) {
                        j[0] = '\0';
                        strncpy(population,i+1,14);
                    } else {
                        strncpy(population,"0",14);
                    } 
                    clean_string(population);
 
                    lat_dd[0] = latitude[0];
                    lat_dd[1] = latitude[1];
                    lat_dd[2] = '\0';
 
                    lat_mm[0] = latitude[2];
                    lat_mm[1] = latitude[3];
                    lat_mm[2] = '\0';

                    lat_ss[0] = latitude[4];
                    lat_ss[1] = latitude[5];
                    lat_ss[2] = '\0';

                    lat_dir[0] = latitude[6];
                    lat_dir[1] = '\0';

                    long_dd[0] = longitude[0];
                    long_dd[1] = longitude[1];
                    long_dd[2] = longitude[2];
                    long_dd[3] = '\0';
 
                    long_mm[0] = longitude[3];
                    long_mm[1] = longitude[4];
                    long_mm[2] = '\0';
 
                    long_ss[0] = longitude[5];
                    long_ss[1] = longitude[6];
                    long_ss[2] = '\0';
 
                    long_dir[0] = longitude[7];
                    long_dir[1] = '\0';

                    // Now must convert from DD MM SS format to DD MM.MM format so that we
                    // can run it through our conversion routine to Xastir coordinates.
                    (void)sscanf(lat_ss, "%d", &temp1);
                    temp1 = (int)((temp1 / 60.0) * 100 + 0.5);  // Poor man's rounding
                    xastir_snprintf(lat_str, sizeof(lat_str), "%s%s.%02d%s", lat_dd,
                            lat_mm, temp1, lat_dir);
                    coord_lat = convert_lat_s2l(lat_str);

                    (void)sscanf(long_ss, "%d", &temp1);
                    temp1 = (int)((temp1 / 60.0) * 100 + 0.5);  // Poor man's rounding
                    xastir_snprintf(long_str, sizeof(long_str), "%s%s.%02d%s", long_dd,
                            long_mm, temp1, long_dir);
                    coord_lon = convert_lon_s2l(long_str);


                    // Save the min/max extents of the file.  We
                    // should really initially set the extents to
                    // the min/max for the Xastir coordinate system,
                    // but in practice zeroes should work just as
                    // well.
                    if ((coord_lat > (long)bottom_extent) || (bottom_extent == 0l))
                        bottom_extent = coord_lat;
                    if ((coord_lat < (long)top_extent) || (top_extent == 0l))
                        top_extent = coord_lat;
                    if ((coord_lon < (long)left_extent) || (left_extent == 0l))
                        left_extent = coord_lon;
                    if ((coord_lon > (long)right_extent) || (right_extent == 0l))
                        right_extent = coord_lon;


                    // Check whether we're indexing the map
                    if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
                            || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {
                        // Do nothing, we're indexing, not drawing
                    }
                    // Now check whether this lat/lon is within our viewport.  If it
                    // is, draw a text label at that location.
                    else if (coord_lon >= min_lon && coord_lon <= max_lon
                            && coord_lat <= min_lat && coord_lat >= max_lat) {

                        if (debug_level & 16) {
                            fprintf(stderr,"%s\t%s\t%s\t%s\t%s\t%s\t\t",
                                    state, name, type, county, latitude, longitude);
                            fprintf(stderr,"%s %s %s %s\t%s %s %s %s\t\t",
                                    lat_dd, lat_mm, lat_ss, lat_dir, long_dd, long_mm, long_ss, long_dir);
                            fprintf(stderr,"%s\t%s\n", lat_str, long_str);
                        }

                        // Convert to screen coordinates.  Careful
                        // here!  The format conversions you'll need
                        // if you try to compress this into two
                        // lines will get you into trouble.
                        x = coord_lon - x_long_offset;
                        y = coord_lat - y_lat_offset;
                        x = x / scale_x;
                        y = y / scale_y;

                        ok = 1;
                        if (x >  16000) ok = 0;     // Skip this point
                        if (x < -16000) ok = 0;     // Skip this point
                        if (y >  16000) ok = 0;     // Skip this point
                        if (y < -16000) ok = 0;     // Skip this point

                        /* set default symbol */
                        symbol_table = '/';
                        symbol_id = '.'; /* small x */
                        symbol_over = ' ';

                        if (strcasecmp(type,"airport") == 0) {
                            symbol_id = '^';
                            if (scale_y > 100)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"arch") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"area") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"arroyo") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"bar") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"basin") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"bay") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"beach") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"bench") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"bend") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"bridge") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"building") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"canal") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"cape") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"cave") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"cemetery") == 0) {
                            symbol_table = '\\';
                            symbol_id = '+';
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"census") == 0) {
                          /* if (scale_y > 50)*/  /* Census divisions */
                                ok = 0;
                        }
                        else if (strcasecmp(type,"channel") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"church") == 0) {
                            symbol_table = '\\';
                            symbol_id = '+';
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"civil") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"cliff") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"crater") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"crossing") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"dam") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"falls") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"flat") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"forest") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"gap") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"geyser") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"glacier") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"gut") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"harbor") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"hospital") == 0) {
                            symbol_id = 'h';
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"island") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"isthmus") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"lake") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"lava") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"levee") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"locale") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"military") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"mine") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"oilfield") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"other") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"park") == 0) {
                            symbol_table = '\\';
                            symbol_id = ';';
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"pillar") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"plain") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"po") == 0) {
                            symbol_id = ']';
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"ppl") == 0) {
                            symbol_id = '/';
                            if (scale_y > 20000)    // Don't draw cities at zoom higher than 20,000
                                ok = 0;
                            else if (scale_y > 4000) {  // Don't draw cities of less than 20,000
                                if (atoi(population) < 50000) {
                                    ok = 0;
                                }
                            }
                            else if (scale_y > 1500) {  // Don't draw cities of less than 10,000
                                if (atoi(population) < 20000) {
                                    ok = 0;
                                }
                            }
                            else if (scale_y > 750) {  // Don't draw cities of less than 5,000
                                if (atoi(population) < 10000) {
                                    ok = 0;
                                }
                            }
                            else if (scale_y > 200) {   // Don't draw cities of less than 1,000
                                if (atoi(population) < 1000) {
                                    ok = 0;
                                    //fprintf(stderr,"Name: %s\tPopulation: %s\n",name,population);
                                }
                            }
                        }
                        else if (strcasecmp(type,"range") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"rapids") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"reserve") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"reservoir") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"ridge") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"school") == 0) {
                            symbol_id = 'K';
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"sea") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"slope") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"spring") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"stream") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"summit") == 0) {
                            if (scale_y > 100)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"swamp") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"trail") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"tower") == 0) {
                            symbol_id = 'r';
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"tunnel") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"valley") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"well") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"woods") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else {
                            fprintf(stderr,"Something unusual found, Type:%s\tState:%s\tCounty:%s\tName:%s\n",
                                type,state,county,name);
                        }

                        if (ok == 1) {  // If ok to draw it
                            symbol(w, 0, symbol_table, symbol_id, symbol_over, pixmap, 1, x-10, y-10, ' ');
                            draw_nice_string(w, pixmap, 0, x+10, y+5, (char*)name, 0xf, 0x10, strlen(name));
                        }

                    }
                    else {
                        //fprintf(stderr,"Not in viewport.  Coordinates: %ld %ld\n",coord_lat,coord_lon);
                        //fprintf(stderr,"Min/Max Lat: %ld %ld\n",min_lat,max_lat);
                        //fprintf(stderr,"Min/Max Lon: %ld %ld\n",min_lon,max_lon);
                    }
                }
            }
        }   // End of while
        (void)fclose (f);


        // Check whether we're indexing the map
        if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
                || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {

            // We're indexing only.  Save the extents in the index.
            index_update_xastir(filenm, // Filename only
                bottom_extent, // Bottom
                top_extent,    // Top
                left_extent,   // Left
                right_extent); // Right
        }
    }
    else {
        fprintf(stderr,"Couldn't open file: %s\n", file);
        return;
    }
    if (debug_level & 16)
        fprintf(stderr,"Exiting draw_gnis_map\n");
}





// Search for a placename among GNIS files
//
// We need to search a file in the map directory that has the filename
// STATE.gis, where STATE is from the "state" variable passed to us.
// Search for the placename/county/state/type that the user requested.
// Once found, center the map on that location or bring up a response
// dialog that asks whether one wants to go there, and that dialog
// provides info about the place found, with a possible selection
// out of a list of matches.
// Might also need to place a label at that position on the map in
// case that GNIS file isn't currently selected.
//
int locate_place( Widget w, char *name_in, char *state_in, char *county_in,
        char *quad_in, char *type_in, char *filename_in, int follow_case, int get_match ) {
    char file[MAX_FILENAME];        // Complete path/name of GNIS file
    FILE *f;                        // Filehandle of GNIS file
    char line[MAX_FILENAME];        // One line of text from file
    char *i, *j, *k;
    char state[50];
    char state_in2[50];
    char name[200];
    char name_in2[50];
    char type[100];
    char type_in2[50];
    char county[100];
    char county_in2[50];
    char quad[100];
    char quad_in2[100];
    char latitude[15];
    char longitude[15];
    char population[15];
    char lat_dd[3];
    char lat_mm[3];
    char lat_ss[3];
    char lat_dir[2];
    char long_dd[4];
    char long_mm[3];
    char long_ss[3];
    char long_dir[2];
    char lat_str[15];
    char long_str[15];
    int temp1;
    long coord_lon, coord_lat;
    int ok;
    struct stat file_status;
 
 
    strcpy(file,filename_in);
    if (debug_level & 16)
        fprintf(stderr,"File: %s\n",file);


    strcpy(name_in2,name_in);
    strcpy(state_in2,state_in);
    strcpy(county_in2,county_in);
    strcpy(quad_in2,quad_in);
    strcpy(type_in2,type_in);


    // Convert State/Province to upper-case always (they're
    // always upper-case in the GNIS files from USGS.
    to_upper(state_in2);


    if (debug_level & 16)
        fprintf(stderr,"Name:%s\tState:%s\tCounty:%s\tQuad:%s\tType:%s\n",
        name_in,state_in2,county_in,quad_in,type_in);


    // If "Match Case" togglebutton is not set, convert the
    // rest of the keys to upper-case.
    if (!follow_case) {
        to_upper(name_in2);
        to_upper(county_in2);
        to_upper(quad_in2);
        to_upper(type_in2);
    }


    // Check status of the file
    if (stat(file, &file_status) < 0) {
        popup_message( langcode("POPEM00028"), filename_in );
        return(0);
    }
    // Check for regular file
    if (!S_ISREG(file_status.st_mode)) {
        popup_message( langcode("POPEM00028"), filename_in );
        return(0);
    }
    // Attempt to open the file
    f = fopen (file, "r");
    if (f != NULL) {
        while (!feof (f)) {     // Loop through entire file
            if ( get_line (f, line, MAX_FILENAME) ) {  // Snag one line of data
                if (strlen(line) > 0) {


//NOTE:  How do we handle running off the end of "line" while using "index"?
// Short lines here can cause segfaults.


                    // Find State feature resides in
                    i = index(line,',');    // Find ',' after state

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    i[0] = '\0';
                    strncpy(state,line+1,49);
//                    state[strlen(state)-1] = '\0';
                    clean_string(state);

//NOTE:  It'd be nice to take the part after the comma and put it before the rest
// of the text someday, i.e. "Cassidy, Lake".
                    // Find Name
                    j = index(i+1, ',');    // Find ',' after Name.  Note that there may be commas in the name.

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    while ( (j != NULL) && (j[1] != '\"') ) {
                        k = j;
                        j = index(k+1, ',');
                    }

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    j[0] = '\0';
                    strncpy(name,i+2,199);
//                    name[strlen(name)-1] = '\0';
                    clean_string(name);

                    // Find Type
                    i = index(j+1, ',');

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    i[0] = '\0';
                    strncpy(type,j+2,99);
//                    type[strlen(type)-1] = '\0';
                    clean_string(type);

                    // Find County          // Can there be commas in the county name?
                    j = index(i+1, ',');

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    j[0] = '\0';
                    strncpy(county,i+2,99);
//                    county[strlen(county)-1] = '\0';
                    clean_string(county);

                    // Find ?
                    i = index(j+1, ',');

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    i[0] = '\0';

                    // Find ?
                    j = index(i+1, ',');

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    j[0] = '\0';

                    // Find latitude (DDMMSSN)
                    i = index(j+1, ',');

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    i[0] = '\0';
                    strncpy(latitude,j+2,14);
//                    latitude[strlen(latitude)-1] = '\0';
                    clean_string(latitude);

                    // Find longitude (DDDMMSSW)
                    j = index(i+1, ',');

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    j[0] = '\0';
                    strncpy(longitude,i+2,14);
//                    longitude[strlen(longitude)-1] = '\0';
                    clean_string(longitude);

                    // Find another latitude
                    i = index(j+1, ',');

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    i[0] = '\0';

                    // Find another longitude
                    j = index(i+1, ',');

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    j[0] = '\0';

                    // Find ?
                    i = index(j+1, ',');

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    i[0] = '\0';

                    // Find ?
                    j = index(i+1, ',');

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    j[0] = '\0';

                    // Find ?
                    i = index(j+1, ',');

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    i[0] = '\0';

                    // Find ?
                    j = index(i+1, ',');

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    j[0] = '\0';

                    // Find altitude
                    i = index(j+1, ',');

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    i[0] = '\0';

                    // Find population
                    j = index(i+1, ',');

                    if (j == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    j[0] = '\0';
                    strncpy(population,i+2,14);
//                    population[strlen(population)-1] = '\0';
                    clean_string(population);
 
                    // Find quad name (last field)
                    i = index(j+1, '"');

                    if (i == NULL) {    // Comma not found
                        continue;   // Skip line
                    }

                    i[0] = '\0';
                    strncpy(quad,j+2,14);
//                    quad[strlen(quad)] = '\0';
                    clean_string(quad);


                    // If "Match Case" togglebutton is not set, convert
                    // the data to upper-case before we do our compare.
                    if (!follow_case) {
                        to_upper(name);
                        to_upper(state);
                        to_upper(county);
                        to_upper(quad);
                        to_upper(type);
                    }


// Still need to code for the "Match Exact" togglebutton.


                    // Now compare the input variables with those we've
                    // parsed.  If a match, bring up a list of items which
                    // match.
                    //
                    ok = 1;
                    if (get_match) {    // Looking for exact match
                        if (name_in2[0] != '\0')
                            if (strcmp(name,name_in2) != 0)
                                ok = 0;
                        if (state_in2[0] != '\0')
                            if (strcmp(state,state_in2) != 0)
                                ok = 0;
                        if (county_in2[0] != '\0')
                            if (strcmp(county,county_in2) != 0)
                                ok = 0;
                        if (quad_in2[0] != '\0')
                            if (strcmp(quad,quad_in2) != 0)
                                ok = 0;
                        if (type_in2[0] != '\0')
                            if (strcmp(type,type_in2) != 0)
                                ok = 0;
                    }
                    else {  // Look for substring in file, not exact match
                        if (name_in2[0] != '\0')
                            if (strstr(name,name_in2) == NULL)
                                ok = 0;
                        if (state_in2[0] != '\0')
                            if (strstr(state,state_in2) == NULL)
                                ok = 0;
                        if (county_in2[0] != '\0')
                            if (strstr(county,county_in2) == NULL)
                                ok = 0;
                        if (quad_in2[0] != '\0')
                            if (strstr(quad,quad_in2) == NULL)
                                ok = 0;
                        if (type_in2[0] != '\0')
                            if (strstr(type,type_in2) == NULL)
                                ok = 0;
                    }


                    if (ok) {
                        if (debug_level & 16)
                            fprintf(stderr,"Match: %s,%s,%s,%s\n",name,state,county,type);

                        popup_message( langcode("POPEM00029"), name );

                        lat_dd[0] = latitude[0];
                        lat_dd[1] = latitude[1];
                        lat_dd[2] = '\0';
 
                        lat_mm[0] = latitude[2];
                        lat_mm[1] = latitude[3];
                        lat_mm[2] = '\0';

                        lat_ss[0] = latitude[4];
                        lat_ss[1] = latitude[5];
                        lat_ss[2] = '\0';

                        lat_dir[0] = latitude[6];
                        lat_dir[1] = '\0';

                        long_dd[0] = longitude[0];
                        long_dd[1] = longitude[1];
                        long_dd[2] = longitude[2];
                        long_dd[3] = '\0';
 
                        long_mm[0] = longitude[3];
                        long_mm[1] = longitude[4];
                        long_mm[2] = '\0';
 
                        long_ss[0] = longitude[5];
                        long_ss[1] = longitude[6];
                        long_ss[2] = '\0';
 
                        long_dir[0] = longitude[7];
                        long_dir[1] = '\0';

                        // Now must convert from DD MM SS format to DD MM.MM format so that we
                        // can run it through our conversion routine to Xastir coordinates.
                        (void)sscanf(lat_ss, "%d", &temp1);
                        temp1 = (int)((temp1 / 60.0) * 100 + 0.5);  // Poor man's rounding
                        xastir_snprintf(lat_str, sizeof(lat_str), "%s%s.%02d%s", lat_dd,
                                lat_mm, temp1, lat_dir);
                        coord_lat = convert_lat_s2l(lat_str);

                        (void)sscanf(long_ss, "%d", &temp1);
                        temp1 = (int)((temp1 / 60.0) * 100 + 0.5);  // Poor man's rounding
                        xastir_snprintf(long_str, sizeof(long_str), "%s%s.%02d%s", long_dd,
                                long_mm, temp1, long_dir);
                        coord_lon = convert_lon_s2l(long_str);
                        set_map_position(w, coord_lat, coord_lon);
                        return(1);  // We found a match
                    }
                }
            }
        }
    } else {
        popup_message( langcode("POPEM00028"), filename_in );
    }

    return(0);  // We didn't find a match
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

void draw_geo_image_map (Widget w, char *dir, char *filenm, int destination_pixmap) {
    uid_t user_id;
    struct passwd *user_info;
    char username[20];
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

//#define TIMING_DEBUG
#ifdef TIMING_DEBUG
    time_mark(1);
#endif  // TIMING_DEBUG

    // Get user info
    user_id=getuid();
    user_info=getpwuid(user_id);
    // Get my login name
    strcpy(username,user_info->pw_name);

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
        xastir_snprintf(map_it, sizeof(map_it), langcode ("BBARSTA039"), filenm);
    }
    else {
        xastir_snprintf(map_it, sizeof(map_it), langcode ("BBARSTA028"), filenm);
    }
    statusline(map_it,0);       // Loading/Indexing ...


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

        return; // Done indexing this file
    }


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
    } else if (debug_level & 16) {
        fprintf(stderr,"Loading imagemap: %s\n", file);
        fprintf(stderr,"\nImage: %s\n", file);
        fprintf(stderr,"Image size %d %d\n", geo_image_width, geo_image_height);
        fprintf(stderr,"XX: %ld YY:%ld Sx %f %d Sy %f %d\n", map_c_L, map_c_T, map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));
    }


    atb.valuemask = 0;


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

        xastir_snprintf(local_filename, sizeof(local_filename), "/var/tmp/xastir_%s_map.%s",
                username,ext);

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

    image = ReadImage(image_info, &exception);

    if (image == (Image *) NULL) {
        MagickWarning(exception.severity, exception.reason, exception.description);
        //fprintf(stderr,"MagickWarning\n");
        return;
    }

    if (debug_level & 16)
        fprintf(stderr,"Color depth is %i \n", (int)image->depth);

    if (image->colorspace != RGBColorspace) {
        puts("TBD: I don't think we can deal with colorspace != RGB");
        if (image)
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    atb.width = image->columns;
    atb.height = image->rows;


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

    if (imagemagick_options.gamma_flag) {
        if (debug_level & 16)
            fprintf(stderr,"gamma=%s\n", gamma);
        GammaImage(image, gamma);
    }

    if (imagemagick_options.contrast != 0) {
        if (debug_level & 16)
            fprintf(stderr,"contrast=%d\n", imagemagick_options.contrast);
        ContrastImage(image, imagemagick_options.contrast);
    }

    if (imagemagick_options.negate != -1) {
        if (debug_level & 16)
            fprintf(stderr,"negate=%d\n", imagemagick_options.negate);
        NegateImage(image, imagemagick_options.negate);
    }

    if (imagemagick_options.equalize) {
        if (debug_level & 16)
            puts("equalize");
        EqualizeImage(image);
    }

    if (imagemagick_options.normalize) {
        if (debug_level & 16)
            puts("normalize");
        NormalizeImage(image);
    }

#if (MagickLibVersion >= 0x0539)
    if (imagemagick_options.level[0] != '\0') {
        if (debug_level & 16)
            fprintf(stderr,"level=%s\n", imagemagick_options.level);
        LevelImage(image, imagemagick_options.level);
    }
#endif  // MagickLibVersion >= 0x0539

    if (imagemagick_options.modulate[0] != '\0') {
        if (debug_level & 16)
            fprintf(stderr,"modulate=%s\n", imagemagick_options.modulate);
        ModulateImage(image, imagemagick_options.modulate);
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
        puts("pixel_pack == NULL!!!");
        if (image)
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    index_pack = GetIndexes(image);
    if (image->storage_class == PseudoClass && !index_pack) {
        puts("PseudoClass && index_pack == NULL!!!");
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





/**********************************************************
 * draw_tiger_map()
 * N0VH
 **********************************************************/
#ifdef HAVE_IMAGEMAGICK
void draw_tiger_map (Widget w) {
    uid_t user_id;
    struct passwd *user_info;
    char username[20];
    char file[MAX_FILENAME];        // Complete path/name of image file
    FILE *f;                        // Filehandle of image file
    char fileimg[MAX_FILENAME];     // Ascii name of image file, read from GEO file
    char tigertmp[MAX_FILENAME*2];  // Used for putting together the tigermap query
    XpmAttributes atb;              // Map attributes after map's read into an XImage
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


    // Get user info
    user_id=getuid();
    user_info=getpwuid(user_id);
    // Get my login name
    strcpy(username,user_info->pw_name);

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
/*		xastir_snprintf(fileimg, sizeof(fileimg), 
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

    xastir_snprintf(local_filename, sizeof(local_filename), "/var/tmp/xastir_%s_map.%s", username,"gif");

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
        puts("TBD: I don't think we can deal with colorspace != RGB");
        if (image)
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    atb.valuemask = 0;
    atb.width = image->columns;
    atb.height = image->rows;

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
        puts("pixel_pack == NULL!!!");
        if (image)
            DestroyImage(image);
        if (image_info)
            DestroyImageInfo(image_info);
        return;
    }

    index_pack = GetIndexes(image);
    if (image->storage_class == PseudoClass && !index_pack) {
        puts("PseudoClass && index_pack == NULL!!!");
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
        fprintf(stderr,"Image size %d %d\n", atb.width, atb.height);
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
    c_y_min = (unsigned long)(tp[0].y_lat + map_y_min * map_c_dy);   // top map inside screen coordinate

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





#ifdef HAVE_LIBGEOTIFF
/***********************************************************
 * read_fgd_file()
 *
 * Read in the "*.fgd" file associated with the geoTIFF
 * file.  Get the corner points from it and return.  If
 * no fgd file exists for this map, return a 0.
 ***********************************************************/
int read_fgd_file ( char* tif_filename,
                    float* f_west_bounding,
                    float* f_east_bounding,
                    float* f_north_bounding,
                    float* f_south_bounding)
{
    char fgd_file[MAX_FILENAME];/* Complete path/name of .fgd file */
    FILE *fgd;                  /* Filehandle of .fgd file */
    char line[MAX_FILENAME];    /* One line from .fgd file */
    int length;
    char *ptr;                  /* Substring pointer */
    int num_coordinates = 0;


    /* Read the .fgd file to find corners of the map neat-line */
    strcpy( fgd_file, tif_filename);
    length = strlen(fgd_file);

    /* Change the extension to ".fgd" */
    fgd_file[length-3] = 'f';
    fgd_file[length-2] = 'g';
    fgd_file[length-1] = 'd';

    if (debug_level & 512)
        fprintf(stderr,"%s\n",fgd_file);

    /*
     * Search for the WEST/EAST/NORTH/SOUTH BOUNDING COORDINATES
     * in the .fgd file.
     */
    fgd = fopen (fgd_file, "r");

    // Try an alternate path (../metadata/ subdirectory) if the first path didn't work
    // This allows working with USGS maps directly from CDROM
    if (fgd == NULL) {
        get_alt_fgd_path(fgd_file, sizeof(fgd_file) );

        if (debug_level & 512)
            fprintf(stderr,"%s\n",fgd_file);

        fgd = fopen (fgd_file, "r");
    }

    if (fgd != NULL)
    {
        while ( ( !feof (fgd) ) && ( num_coordinates < 4 ) )
        {
            get_line (fgd, line, MAX_FILENAME);

            if (*f_west_bounding == 0.0)
            {
                if ( ( (ptr = strstr(line, "WEST BOUNDING COORDINATE:") ) != NULL)
                        || ( (ptr = strstr(line, "West_Bounding_Coordinate:") ) != NULL) )
                {
                    sscanf (ptr + 25, " %f", f_west_bounding);
                    if (debug_level & 512)
                        fprintf(stderr,"West Bounding:  %f\n",*f_west_bounding);
                    num_coordinates++;
                }
            }

            else if (*f_east_bounding == 0.0)
            {
                if ( ( (ptr = strstr(line, "EAST BOUNDING COORDINATE:") ) != NULL)
                        || ( (ptr = strstr(line, "East_Bounding_Coordinate:") ) != NULL) )
                {
                    sscanf (ptr + 25, " %f", f_east_bounding);
                    if (debug_level & 512)
                        fprintf(stderr,"East Bounding:  %f\n",*f_east_bounding);
                    num_coordinates++;
                }
            }

            else if (*f_north_bounding == 0.0)
            {
                if ( ( (ptr = strstr(line, "NORTH BOUNDING COORDINATE:") ) != NULL)
                        || ( (ptr = strstr(line, "North_Bounding_Coordinate:") ) != NULL) )
                {
                    sscanf (ptr + 26, " %f", f_north_bounding);
                    if (debug_level & 512)
                        fprintf(stderr,"North Bounding: %f\n",*f_north_bounding);
                    num_coordinates++;
                }
            }

            else if (*f_south_bounding == 0.0)
            {
                if ( ( (ptr = strstr(line, "SOUTH BOUNDING COORDINATE:") ) != NULL)
                        || ( (ptr = strstr(line, "South_Bounding_Coordinate:") ) != NULL) )
                {
                    sscanf (ptr + 26, " %f", f_south_bounding);
                    if (debug_level & 512)
                        fprintf(stderr,"South Bounding: %f\n",*f_south_bounding);
                    num_coordinates++;
                }
            }

        }
        fclose (fgd);
    }
    else
    {
        if (debug_level & 512)
            fprintf(stderr,"Couldn't open '.fgd' file, assuming no map collar to chop %s\n",
                    tif_filename);
        return(0);
    }


    /*
     * We should now have exactly four bounding coordinates.
     * These specify the map neat-line corners.  We can use
     * them to chop off the white collar from around the map.
     */
    if (num_coordinates != 4)
    {
        fprintf(stderr,"Couldn't find 4 bounding coordinates in '.fgd' file, map %s\n",
                tif_filename);
        return(0);
    }


    if (debug_level & 512) {
        fprintf(stderr,"%f %f %f %f\n",
        *f_south_bounding,
        *f_north_bounding,
        *f_west_bounding,
        *f_east_bounding);
    }

    return(1);    /* Successful */
}





// Check the "GTIFProj4*" functions to see if we can use
// datum.c instead for any of them.  These calls are all in
// draw_geotiff_image_map().  It looks like they deal with
// converting from pixels to coordinates and vice-versa, so we
// probably still need them.  Perhaps we can borrow the code for
// that and do it ourselves, to get rid of the extra library?

/***********************************************************
 * draw_geotiff_image_map()
 *
 * Here's where we handle geoTIFF files, such as USGS DRG
 * topo maps.  The .fgd file gives us the lat/lon of the map
 * neat-line corners for USGS maps.  We use this info to
 * chop off the white map border.  If no .fgd file is present,
 * we assume there is no map collar to be cropped and display
 * every pixel.
 * We also translate from the map datum to WGS84.  We use
 * libgeotiff/libtiff/libproj for these operations.

 * TODO:
 * Provide support for datums other than NAD27/NAD83/WGS84.
 * Libproj doesn't currently support many datums.
 *
 * Provide support for handling different map projections.
 * Perhaps by reprojecting the map data and storing it on
 * disk in another format.
 *
 * Select 'o', 'f', 'k', or 'c' maps based on zoom level.
 * Might also put some hysteresis in this so that it keeps
 * the current type of map through one extra zoom each way.
 * 'c': Good from x256 to x064.
 * 'f': Good from x128 to x032.  Not very readable at x128.
 * 'k': Good from x??? to x???.
 * 'o': Good from x064 to x004.  Not very readable at x64.
 ***********************************************************/
void draw_geotiff_image_map (Widget w, char *dir, char *filenm, int destination_pixmap) {
    char file[MAX_FILENAME];    /* Complete path/name of image file */
    TIFF *tif = (TIFF *) 0;     /* Filehandle for tiff image file */
    GTIF *gtif = (GTIF *) 0;    /* GeoKey-level descriptor */
    /* enum { VERSION = 0, MAJOR, MINOR }; */
    int versions[3];
    uint32 width;               /* Width of the image */
    uint32 height;              /* Height of the image */
    uint16 bitsPerSample;       /* Should be 8 for USGS DRG's */
    uint16 samplesPerPixel = 1; /* Should be 1 for USGS DRG's.  Some maps
                                    don't have this tag so we default to 1 */
    uint32 rowsPerStrip;        /* Should be 1 for USGS DRG's */
    uint16 planarConfig;        /* Should be 1 for USGS DRG's */
    uint16 photometric;         /* DRGs are RGB (2) */
    int    bytesPerRow;            /* Bytes per scanline row of tiff file */
    GTIFDefn defn;              /* Stores geotiff details */
    u_char *imageMemory;        /* Fixed pointer to same memory area */
    uint32 row;                 /* My row counter for the loop */
    int num_colors;             /* Number of colors in the geotiff colormap */
    uint16 *red_orig, *green_orig, *blue_orig; /* Used for storing geotiff colors */
    XColor my_colors[256];      /* Used for translating colormaps */
    unsigned long west_bounding = 0;
    unsigned long east_bounding = 0;
    unsigned long north_bounding = 0;
    unsigned long south_bounding = 0;
    float f_west_bounding = 0.0;
    float f_east_bounding = 0.0;
    float f_north_bounding = 0.0;
    float f_south_bounding = 0.0;

    unsigned long west_bounding_wgs84 = 0;
    unsigned long east_bounding_wgs84 = 0;
    unsigned long north_bounding_wgs84 = 0;
    unsigned long south_bounding_wgs84 = 0;

    float f_NW_x_bounding;
    float f_NW_y_bounding;
    float f_NE_x_bounding;
    float f_NE_y_bounding;
    float f_SW_x_bounding;
    float f_SW_y_bounding;
    float f_SE_x_bounding;
    float f_SE_y_bounding;

    unsigned long NW_x_bounding_wgs84 = 0;
    unsigned long NW_y_bounding_wgs84 = 0;
    double f_NW_x_bounding_wgs84 = 0.0;
    double f_NW_y_bounding_wgs84 = 0.0;

    unsigned long NE_x_bounding_wgs84 = 0;
    unsigned long NE_y_bounding_wgs84 = 0;
    double f_NE_x_bounding_wgs84 = 0.0;
    double f_NE_y_bounding_wgs84 = 0.0;

    unsigned long SW_x_bounding_wgs84 = 0;
    unsigned long SW_y_bounding_wgs84 = 0;
    double f_SW_x_bounding_wgs84 = 0.0;
    double f_SW_y_bounding_wgs84 = 0.0;

    unsigned long SE_x_bounding_wgs84 = 0;
    unsigned long SE_y_bounding_wgs84 = 0;
    double f_SE_x_bounding_wgs84 = 0.0;
    double f_SE_y_bounding_wgs84 = 0.0;

    int NW_x = 0;               /* Store pixel values for map neat-line */
    int NW_y = 0;               /* ditto */
    int NE_x = 0;               /* ditto */
    int NE_y = 0;               /* ditto */
    int SW_x = 0;               /* ditto */
    int SW_y = 0;               /* ditto */
    int SE_x = 0;               /* ditto */
    int SE_y = 0;               /* ditto */
    int left_crop;              /* Pixel cropping value */
    int right_crop;             /* Pixel cropping value */
    int top_crop;               /* Pixel cropping value */
    int bottom_crop;            /* Pixel cropping value */
    double xxx, yyy;            /* LFM: needs more accuracy here */
    register long sxx, syy;              /* X Y screen plot positions          */
    float steph;
    register float stepw;
    int stepwc, stephc;
    char map_it[MAX_FILENAME];           /* Used to hold filename for status line */
    int have_fgd;               /* Tells where we have an associated *.fgd file */
    //short datum;
    char *datum_name;           /* Points to text name of datum */
    //double *GeoTie;
    int crop_it = 0;            /* Flag which tells whether the image should be cropped */

    register uint32 column;

    float xastir_left_x_increment;
    float left_x_increment;
    float xastir_left_y_increment;
    float left_y_increment;
    float xastir_right_x_increment;
    float right_x_increment;
    float xastir_right_y_increment;
    float right_y_increment;
    float xastir_top_y_increment;
    float top_y_increment;
    float xastir_bottom_y_increment;
    float bottom_y_increment;
    float xastir_avg_y_increment;
    float avg_y_increment;
    int row_offset;
    register unsigned long current_xastir_left;
    unsigned long current_xastir_right;
    register uint32 current_left;
    uint32 current_right;
    uint32 current_line_width;
    register unsigned long xastir_current_y;
    register uint32 column_offset;
    register unsigned long xastir_current_x;
    double *PixelScale;
    int have_PixelScale;
    uint16 qty;
    int SkipRows;
    unsigned long view_min_x, view_max_x;
    unsigned long view_min_y, view_max_y;

    register unsigned long xastir_total_y;
    int NW_line_offset;
    int NE_line_offset;
    int NW_xastir_x_offset;
    int NE_xastir_x_offset;
    int NW_xastir_y_offset;
    int NW_x_offset;
    int NE_x_offset;
    float xastir_avg_left_right_y_increment;
    register float total_avg_y_increment;
    unsigned long view_left_minus_pixel_width;
    unsigned long view_top_minus_pixel_height;



    if (debug_level & 16)
        fprintf(stderr,"%s/%s\n", dir, filenm);


    xastir_snprintf(file, sizeof(file), "%s/%s", dir, filenm);

    /* Check whether we have an associated *.fgd file.  This
     * file contains the neat-line corner points for USGS DRG
     * maps, which allows us to chop off the map collar.
     */
    have_fgd = read_fgd_file( file,
                              &f_west_bounding,
                              &f_east_bounding,
                              &f_north_bounding,
                              &f_south_bounding );

    /*
     * If we are able to read the fgd file then we have the lat/lon
     * corner points in floating point variables.  If there isn't
     * an fgd file then we must get the info from the geotiff
     * tags themselves and we assume that there's no map collar to
     * chop off.
     */


    /*
     * What we NEED to do (implemented a bit later in this function
     * in order to support geotiff files created with other map
     * datums, is to open up the geotiff file and get the map datum
     * used for the data.  Then convert the corner points to WGS84
     * and check to see whether the image is inside our viewport.
     * Some USGS geotiff maps have map data in NAD83 datum and the
     * .fgd file incorrectly specifying NAD27 datum.  There are also
     * some USGS geotiff maps created with WGS84 datum.
     */


    /* convert_to_xastir_coordinates( x,y,longitude,latitude ); */
    if (have_fgd)   /* Could be a USGS file */
    {
        int temp_ok1, temp_ok2;

        if (debug_level & 16) {
            fprintf(stderr,"FGD:  W:%f  E:%f  N:%f  S:%f\n",
                f_west_bounding,
                f_east_bounding,
                f_north_bounding,
                f_south_bounding);
        }

        crop_it = 1;        /* The map collar needs to be cropped */

        temp_ok1 = convert_to_xastir_coordinates(  &west_bounding,
                                        &north_bounding,
                                        f_west_bounding,
                                        f_north_bounding );


        temp_ok2 = convert_to_xastir_coordinates(  &east_bounding,
                                        &south_bounding,
                                        f_east_bounding,
                                        f_south_bounding );

        if (!temp_ok1 || !temp_ok2) {
            fprintf(stderr,"draw_geotiff_image_map: problem converting from lat/lon\n");
            return;
        }


        /*
         * Check whether map is inside our current view.  It'd be
         * good to do a datum conversion first, but we don't know
         * what the datum is by this point in the code.  I'm just
         * doing this check here for speed, so that I can eliminate
         * maps that aren't even close to our viewport area, without
         * having to open those map files.  All other maps that pass
         * this test (at the next go-around later in the code) must
         * have their corner points datum-shifted so that we can
         * REALLY tell whether a map fits within the viewport.
         *
         * Perhaps add a bit to the corners (the max datum shift?)
         * to do our quick check?  I decided to add about 10 seconds
         * to the map edges, which equates to 1000 in the Xastir
         * coordinate system.  That should be greater than any datum
         * shift in North America for USGS topos.  I'm artificially
         * inflating the size of the map just for this quick
         * elimination check.
         *
         *   bottom          top             left           right
         */

        // Check whether we're indexing or drawing the map
        if ( (destination_pixmap != INDEX_CHECK_TIMESTAMPS)
            && (destination_pixmap != INDEX_NO_TIMESTAMPS) ) {

            // We're drawing.
            if (!map_visible( south_bounding + 1000,
                              north_bounding - 1000,
                              west_bounding - 1000,
                              east_bounding + 1000 ) ) {
                if (debug_level & 16) {
                    fprintf(stderr,"Map not within current view.\n");
                    fprintf(stderr,"Skipping map: %s\n", file);
                }

                // Map isn't inside our current view.  We're done.
                // Free any memory used and return.
                //
                return;    // Skip this map
            }
        }
    }


    /*
     * At this point the map MAY BE in our current view.
     * We don't know for sure until we do a datum translation
     * on the bounding coordinates and check again.  Note that
     * if there's not an accompanying .fgd file, we don't have
     * the bounding coordinates yet by this point.
     */


    /* Open TIFF descriptor to read GeoTIFF tags */
    tif = XTIFFOpen (file, "r");
    if (!tif)
        return;


    /* Open GTIF Key parser.  Keys will be read at this time */
    gtif = GTIFNew (tif);
    if (!gtif)
    {
        /* Close the TIFF file descriptor */
        XTIFFClose (tif);
        return;
    }


    /*
     * Get the GeoTIFF directory info.  Need this for
     * some of the operations further down in the code.
     */
    GTIFDirectoryInfo (gtif, versions, 0);

    /*
    if (versions[MAJOR] > 1)
    {
        fprintf(stderr,"This file is too new for me\n");
        GTIFFree (gtif);
        XTIFFClose (tif);
        return;
    }
    */


    /* I might want to attempt to avoid the GTIFGetDefn
     * call, as it takes a bit of time per file.  It
     * normalizes the info.  Try getting just the tags
     * or keys that I need individually instead.  I
     * need "defn" for the GTIFProj4ToLatLong calls though.
     */
    if (GTIFGetDefn (gtif, &defn))
    {
        if (debug_level & 16)
            GTIFPrintDefn (&defn, stdout);
    }

 
    /* Fetch a few TIFF fields for this image */
    if ( !TIFFGetField (tif, TIFFTAG_IMAGEWIDTH, &width) ) {
        width = 5493;
        fprintf(stderr,"No width tag found in file, setting it to 5493\n");
    }

    if ( !TIFFGetField (tif, TIFFTAG_IMAGELENGTH, &height) ) {
        height = 6840;
        fprintf(stderr,"No height tag found in file, setting it to 6840\n");
    }


    /*
     * If we don't have an associated .fgd file for this map,
     * check for corner points in the ImageDescription
     * tag (proposed new USGS DRG standard).  Boundary
     * coordinates will be the outside corners of the image
     * unless I can find some other proof.
     * Currently I assume that the map has no
     * map collar to chop off and set the neat-line corners
     * to be the outside corners of the image.
     *
     * NOTE:  For the USGS files (with a map collar), the
     * image must be cropped and rotated and is slightly
     * narrower at one end (top for northern hemisphere, bottom
     * for southern hemisphere).  For other files with no map
     * collar, the image is rectangular but the lat/lon
     * coordinates may be rotated.
     */
    if (!have_fgd)      // Not a USGS map or perhaps a newer spec
    {
        crop_it = 0;        /* Do NOT crop this map image */

        /*
         * Snag and parse ImageDescription tag here.
         */

        /* Code goes here for getting ImageDescription tag... */


        /* Figure out the bounding coordinates for this map */
        if (debug_level & 16)
            fprintf(stderr,"\nCorner Coordinates:\n");

        /* Find lat/lon for NW corner of image */
        xxx = 0.0;
        yyy = 0.0;
        if ( GTIFImageToPCS( gtif, &xxx, &yyy ) )   // Do all 4 of these in one call?
        {
            if (debug_level & 16) {
                fprintf(stderr,"%-13s ", "Upper Left" );
                fprintf(stderr,"(%11.3f,%11.3f)\n", xxx, yyy );
            }
        }
        if ( GTIFProj4ToLatLong( &defn, 1, &xxx, &yyy ) )   // Do all 4 of these in one call?
        {
            if (debug_level & 16) {
                fprintf(stderr,"  (%s,", GTIFDecToDMS( xxx, "Long", 2 ) );
                fprintf(stderr,"%s)\n", GTIFDecToDMS( yyy, "Lat", 2 ) );
                fprintf(stderr,"%f  %f\n", xxx, yyy);
            }
        }
        f_NW_x_bounding = (float)xxx;
        f_NW_y_bounding = (float)yyy;


        /* Find lat/lon for NE corner of image */
        xxx = width - 1;
        yyy = 0.0;
        if ( GTIFImageToPCS( gtif, &xxx, &yyy ) )
        {
            if (debug_level & 16) {
                fprintf(stderr,"%-13s ", "Lower Right" );
                fprintf(stderr,"(%11.3f,%11.3f)\n", xxx, yyy );
            }
        }
        if ( GTIFProj4ToLatLong( &defn, 1, &xxx, &yyy ) )
        {
            if (debug_level & 16) {
                fprintf(stderr,"  (%s,", GTIFDecToDMS( xxx, "Long", 2 ) );
                fprintf(stderr,"%s)\n", GTIFDecToDMS( yyy, "Lat", 2 ) );
                fprintf(stderr,"%f  %f\n", xxx, yyy);
            }
        }
        f_NE_x_bounding = (float)xxx;
        f_NE_y_bounding = (float)yyy;

        /* Find lat/lon for SW corner of image */
        xxx = 0.0;
        yyy = height - 1;
        if ( GTIFImageToPCS( gtif, &xxx, &yyy ) )
        {
            if (debug_level & 16) {
                fprintf(stderr,"%-13s ", "Lower Right" );
                fprintf(stderr,"(%11.3f,%11.3f)\n", xxx, yyy );
            }
        }
        if ( GTIFProj4ToLatLong( &defn, 1, &xxx, &yyy ) )
        {
            if (debug_level & 16) {
                fprintf(stderr,"  (%s,", GTIFDecToDMS( xxx, "Long", 2 ) );
                fprintf(stderr,"%s)\n", GTIFDecToDMS( yyy, "Lat", 2 ) );
                fprintf(stderr,"%f  %f\n", xxx, yyy);
            }
        }
        f_SW_x_bounding = (float)xxx;
        f_SW_y_bounding = (float)yyy;

        /* Find lat/lon for SE corner of image */
        xxx = width - 1;
        yyy = height - 1;
        if ( GTIFImageToPCS( gtif, &xxx, &yyy ) )
        {
            if (debug_level & 16) {
                fprintf(stderr,"%-13s ", "Lower Right" );
                fprintf(stderr,"(%11.3f,%11.3f)\n", xxx, yyy );
            }
        }
        if ( GTIFProj4ToLatLong( &defn, 1, &xxx, &yyy ) )
        {
            if (debug_level & 16) {
                fprintf(stderr,"  (%s,", GTIFDecToDMS( xxx, "Long", 2 ) );
                fprintf(stderr,"%s)\n", GTIFDecToDMS( yyy, "Lat", 2 ) );
                fprintf(stderr,"%f  %f\n", xxx, yyy);
            }
        }
        f_SE_x_bounding = (float)xxx;
        f_SE_y_bounding = (float)yyy;
    }

    // Handle special USGS geoTIFF case here.  We only have
    // four boundaries because the edges are aligned with
    // lat/long.
    else    // have_fgd
    {
        f_NW_x_bounding = f_west_bounding;
        f_NW_y_bounding = f_north_bounding;

        f_SW_x_bounding = f_west_bounding;
        f_SW_y_bounding = f_south_bounding;

        f_NE_x_bounding = f_east_bounding;
        f_NE_y_bounding = f_north_bounding;

        f_SE_x_bounding = f_east_bounding;
        f_SE_y_bounding = f_south_bounding;
    }


    // Fill in the wgs84 variables so we can do a datum
    // conversion but keep our original values also.
    f_NW_x_bounding_wgs84 = f_NW_x_bounding;
    f_NW_y_bounding_wgs84 = f_NW_y_bounding;

    f_SW_x_bounding_wgs84 = f_SW_x_bounding;
    f_SW_y_bounding_wgs84 = f_SW_y_bounding;

    f_NE_x_bounding_wgs84 = f_NE_x_bounding;
    f_NE_y_bounding_wgs84 = f_NE_y_bounding;

    f_SE_x_bounding_wgs84 = f_SE_x_bounding;
    f_SE_y_bounding_wgs84 = f_SE_y_bounding;


    /* Get the datum */
    // GTIFKeyGet( gtif, GeogGeodeticDatumGeoKey, &datum, 0, 1 );
    // if (debug_level & 16)
    //     fprintf(stderr,"GeogGeodeticDatumGeoKey: %d\n", datum );


    /* Get the tiepoints (in UTM coordinates always?)
     * In our case they look like:
     *
     * 0.000000         Y
     * 0.000000         X
     * 0.000000         Z
     * 572983.025771    Y in UTM (longitude for some maps?)
     * 5331394.085064   X in UTM (latitude for some maps?)
     * 0.000000         Z
     *
     */
    /*
    if (debug_level & 16) {
        fprintf(stderr,"Tiepoints:\n");
        if ( TIFFGetField( tif, TIFFTAG_GEOTIEPOINTS, &qty, &GeoTie ) ) {
            for ( i = 0; i < qty; i++ ) {
                fprintf(stderr,"%f\n", *(GeoTie + i) );
            }
        }
    }
    */


    /* Get the geotiff horizontal datum name */
    if ( defn.Datum != 32767 ) {
        GTIFGetDatumInfo( defn.Datum, &datum_name, NULL );
        if (debug_level & 16)
            fprintf(stderr,"Datum: %d/%s\n", defn.Datum, datum_name );
    }


    /*
     * Perform a datum shift on the bounding coordinates before we
     * check whether the map is inside our viewport.  At the moment
     * this is still hard-coded to NAD27 datum.  If the map is already
     * in WGS84 or NAD83 datum, skip the datum conversion code.
     */
    if (   (defn.Datum != 6030)     /* DatumE_WGS84 */
        && (defn.Datum != 6326)     /*  Datum_WGS84 */
        && (defn.Datum != 6269) )   /* Datum_North_American_Datum_1983 */ {

        if (debug_level & 16)
            fprintf(stderr,"***** Attempting Datum Conversions\n");


        // This code uses datum.h/datum.c to do the conversion
        // instead of the proj.4 library as we had before.
        // Here we assume that if it's not one of the three datums
        // listed above, it's NAD27.

        // Convert NW corner to WGS84
        wgs84_datum_shift(TO_WGS_84,
            &f_NW_y_bounding_wgs84,
            &f_NW_x_bounding_wgs84,
            D_NAD_27_CONUS);   // NAD27 CONUS

        // Convert NE corner to WGS84
        wgs84_datum_shift(TO_WGS_84,
            &f_NE_y_bounding_wgs84,
            &f_NE_x_bounding_wgs84,
            D_NAD_27_CONUS);   // NAD27 CONUS

        // Convert SW corner to WGS84
        wgs84_datum_shift(TO_WGS_84,
            &f_SW_y_bounding_wgs84,
            &f_SW_x_bounding_wgs84,
            D_NAD_27_CONUS);   // NAD27 CONUS

        // Convert SE corner to WGS84
        wgs84_datum_shift(TO_WGS_84,
            &f_SE_y_bounding_wgs84,
            &f_SE_x_bounding_wgs84,
            D_NAD_27_CONUS);   // NAD27 CONUS (131)
    }
    else
        if (debug_level & 16)
            fprintf(stderr,"***** Skipping Datum Conversion\n");


    /*
     * Convert new datum-translated bounding coordinates to the
     * Xastir coordinate system.
     * convert_to_xastir_coordinates( x,y,longitude,latitude )
     */
    // NW corner
    if (!convert_to_xastir_coordinates(  &NW_x_bounding_wgs84,
                                        &NW_y_bounding_wgs84,
                                        (float)f_NW_x_bounding_wgs84,
                                        (float)f_NW_y_bounding_wgs84 ) ) {
        fprintf(stderr,"draw_geotiff_image_map: Problem converting from lat/lon\n");
        fprintf(stderr,"Did you follow the instructions for installing PROJ?\n");
        return;
    }

    // NE corner
    if (!convert_to_xastir_coordinates(  &NE_x_bounding_wgs84,
                                        &NE_y_bounding_wgs84,
                                        (float)f_NE_x_bounding_wgs84,
                                        (float)f_NE_y_bounding_wgs84 ) ) {
        fprintf(stderr,"draw_geotiff_image_map: Problem converting from lat/lon\n");
        fprintf(stderr,"Did you follow the instructions for installing PROJ?\n");
 
        return;
    }

    // SW corner
    if (!convert_to_xastir_coordinates(  &SW_x_bounding_wgs84,
                                        &SW_y_bounding_wgs84,
                                        (float)f_SW_x_bounding_wgs84,
                                        (float)f_SW_y_bounding_wgs84 ) ) {
        fprintf(stderr,"draw_geotiff_image_map: Problem converting from lat/lon\n");
        fprintf(stderr,"Did you follow the instructions for installing PROJ?\n");
 
        return;
    }

    // SE corner
    if (!convert_to_xastir_coordinates(  &SE_x_bounding_wgs84,
                                        &SE_y_bounding_wgs84,
                                        (float)f_SE_x_bounding_wgs84,
                                        (float)f_SE_y_bounding_wgs84 ) ) {
        fprintf(stderr,"draw_geotiff_image_map: Problem converting from lat/lon\n");
        fprintf(stderr,"Did you follow the instructions for installing PROJ?\n");
 
        return;
    }


    /*
     * Check whether map is inside our current view.  These
     * are the real datum-shifted bounding coordinates now,
     * so this is the final decision as to whether the map
     * should be loaded.
     */

    // Find the largest dimensions
    if (NW_y_bounding_wgs84 <= NE_y_bounding_wgs84)
        north_bounding_wgs84 = NW_y_bounding_wgs84;
    else
        north_bounding_wgs84 = NE_y_bounding_wgs84;

    if (NW_x_bounding_wgs84 <= SW_x_bounding_wgs84)
        west_bounding_wgs84 = NW_x_bounding_wgs84;
    else
        west_bounding_wgs84 = SW_x_bounding_wgs84;

    if (SW_y_bounding_wgs84 >= SE_y_bounding_wgs84)
        south_bounding_wgs84 = SW_y_bounding_wgs84;
    else
        south_bounding_wgs84 = SE_y_bounding_wgs84;

    if (NE_x_bounding_wgs84 >= SE_x_bounding_wgs84)
        east_bounding_wgs84 = NE_x_bounding_wgs84;
    else
        east_bounding_wgs84 = SE_x_bounding_wgs84;


    // Check whether we're indexing or drawing the map
    if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
            || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {

        xastir_snprintf(map_it, sizeof(map_it), langcode ("BBARSTA039"), filenm);
        statusline(map_it,0);       // Indexing ...

        // We're indexing only.  Save the extents in the index.
        index_update_xastir(filenm, // Filename only
            south_bounding_wgs84,   // Bottom
            north_bounding_wgs84,   // Top
            west_bounding_wgs84,    // Left
            east_bounding_wgs84);   // Right

        //Free any memory used and return
        /* We're finished with the geoTIFF key parser, so get rid of it */
        GTIFFree (gtif);

        /* Close the TIFF file descriptor */
        XTIFFClose (tif);
 
        return; // Done indexing this file
    }
    else {
        xastir_snprintf(map_it, sizeof(map_it), langcode ("BBARSTA028"), filenm);
        statusline(map_it,0);       // Loading ...
    }




    // bottom top left right
    if (!map_visible( south_bounding_wgs84,
                         north_bounding_wgs84,
                         west_bounding_wgs84,
                         east_bounding_wgs84 ) )
    {
        if (debug_level & 16) {
            fprintf(stderr,"Map not within current view.\n");
            fprintf(stderr,"Skipping map: %s\n", file);
        }

        /*
         * Map isn't inside our current view.  We're done.
         * Free any memory used and return
         */

        /* We're finished with the geoTIFF key parser, so get rid of it */
        GTIFFree (gtif);

        /* Close the TIFF file descriptor */
        XTIFFClose (tif);

        return;         /* Skip this map */
    }

/*
From running in debug mode:
            Width: 5493
           Height: 6840
   Rows Per Strip: 1
  Bits Per Sample: 8
Samples Per Pixel: 1
    Planar Config: 1
*/


    /* Fetch a few TIFF fields for this image */
    if ( !TIFFGetField (tif, TIFFTAG_PHOTOMETRIC, &photometric) ) {
        photometric = PHOTOMETRIC_RGB;
        fprintf(stderr,"No photometric tag found in file, setting it to RGB\n");
    }
    if ( !TIFFGetField (tif, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip) ) {
        rowsPerStrip = 1;
        fprintf(stderr,"No rowsPerStrip tag found in file, setting it to 1\n");
    }

    if ( !TIFFGetField (tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample) ) {
        bitsPerSample = 8;
        fprintf(stderr,"No bitsPerSample tag found in file, setting it to 8\n");
    }

    if ( !TIFFGetField (tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel) ) {
        samplesPerPixel = 1;
        fprintf(stderr,"No samplesPerPixel tag found in file, setting it to 1\n");
    }

    if ( !TIFFGetField (tif, TIFFTAG_PLANARCONFIG, &planarConfig) ) {
        planarConfig = 1;
        fprintf(stderr,"No planarConfig tag found in file, setting it to 1\n");
    }


    if (debug_level & 16) {
        fprintf(stderr,"            Width: %ld\n", width);
        fprintf(stderr,"           Height: %ld\n", height);
        fprintf(stderr,"      Photometric: %d\n", photometric);
        fprintf(stderr,"   Rows Per Strip: %ld\n", rowsPerStrip);
        fprintf(stderr,"  Bits Per Sample: %d\n", bitsPerSample);
        fprintf(stderr,"Samples Per Pixel: %d\n", samplesPerPixel);
        fprintf(stderr,"    Planar Config: %d\n", planarConfig);
    }


    /*
     * Check for properly formatted geoTIFF file.  If it isn't
     * in the standard format we're looking for, spit out an
     * error message and return.
     *
     * Should we also check compression method here?
     */
    /* if ( (   rowsPerStrip != 1) */
    if ( (samplesPerPixel != 1)
        || (  bitsPerSample != 8)
        || (   planarConfig != 1) )
    {
        fprintf(stderr,"*** geoTIFF file %s is not in the proper format.\n", file);
        fprintf(stderr,"*** Please reformat it and try again.\n");
        XTIFFClose(tif);
        return;
    }


    if (debug_level & 16)
        fprintf(stderr,"Loading geoTIFF map: %s\n", file);


    /* Put "Loading..." message on status line */
    // Check whether we're indexing or drawing the map
    if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
            || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {
        xastir_snprintf(map_it, sizeof(map_it), langcode ("BBARSTA039"), filenm);
    }
    else {
        xastir_snprintf(map_it, sizeof(map_it), langcode ("BBARSTA028"), filenm);
    }
    statusline(map_it,0);       // Loading/Indexing ...


    /*
     * Snag the original map colors out of the colormap embedded
     * inside the tiff file.
     */
    if (photometric == PHOTOMETRIC_PALETTE) {
        if (!TIFFGetField(tif, TIFFTAG_COLORMAP, &red_orig, &green_orig, &blue_orig))
        {
            TIFFError(TIFFFileName(tif), "Missing required \"Colormap\" tag");
            GTIFFree (gtif);
            XTIFFClose (tif);
            return;
        }
    }

    /* Here are the number of possible colors.  It turns out to
     * be 256 for a USGS geotiff file, of which only the first
     * 13 are used.  Other types of geotiff's may use more
     * colors (and do).  A proposed revision to the USGS DRG spec
     * allows using more colors.
     */
    num_colors = (1L << bitsPerSample);


    /* Print out the colormap info */
    //    if (debug_level & 16) {
    //        int l;
    //
    //        for (l = 0; l < num_colors; l++)
    //            fprintf(stderr,"   %5u: %5u %5u %5u\n",
    //                        l,
    //                        red_orig[l],
    //                        green_orig[l],
    //                        blue_orig[l]);
    //    }


    if (crop_it)    // USGS geoTIFF map
    {
         /*
         * Next:
         * Convert the map neat-line corners to image x/y coordinates.
         * This will give the map neat-line coordinates in pixels.
         * Use this data to chop the image at these boundaries
         * and to stretch the shorter lines to fit a rectangle.
         *
         * Note that at this stage we're using the bounding coordinates
         * that are in the map original datum so that the translations
         * to pixel coordinates will be correct.
         *
         * Note that we already have the datum-shifted values for all
         * the corners in the *_wgs84 variables.  In short:  We use the
         * non datum-shifted values to work with the tiff file, and the
         * datum-shifted values to plot the points in Xastir.
         */

        if (debug_level & 16)
            fprintf(stderr,"\nNW neat-line corner = %f\t%f\n",
                    f_NW_x_bounding,
                    f_NW_y_bounding);

        xxx = (double)f_NW_x_bounding;
        yyy = (double)f_NW_y_bounding;

        /* Convert lat/long to projected coordinates */
        if ( GTIFProj4FromLatLong( &defn, 1, &xxx, &yyy ) )     // Do all 4 in one call?
        {
            if (debug_level & 16)
                fprintf(stderr,"%11.3f,%11.3f\n", xxx, yyy);

            /* Convert from PCS coordinates to image pixel coordinates */
            if ( GTIFPCSToImage( gtif, &xxx, &yyy ) )           // Do all 4 in one call?
            {
                if (debug_level & 16)
                    fprintf(stderr,"X/Y Pixels: %f, %f\n", xxx, yyy);

                NW_x = (int)(xxx + 0.5);    /* Tricky way of rounding */
                NW_y = (int)(yyy + 0.5);    /* Tricky way of rounding */

                if (debug_level & 16)
                    fprintf(stderr,"X/Y Pixels: %d, %d\n", NW_x, NW_y);

                if (NW_x < 0 || NW_y < 0 || NW_x >= (int)width || NW_y >= (int)height) {

                    fprintf(stderr,"\nWarning:  NW Neat-line corner calculated at x:%d, y:%d, %s\n",
                        NW_x, NW_y, filenm);
                    fprintf(stderr,"Limits are: 0,0 and %ld,%ld. Resetting corner position.\n",width,height);
                    fprintf(stderr,"Map may appear in the wrong location or scale incorrectly.\n");

                    if (NW_x < 0)
                        NW_x = 0;

                    if (NW_x >= (int)width)
                        NW_x = width - 1;

                    if (NW_y < 0)
                        NW_y = 0;

                    if (NW_y >= (int)height)
                        NW_y = height -1;

/*
                    //Free any memory used and return
                    // We're finished with the geoTIFF key parser, so get rid of it
                    GTIFFree (gtif);

                    // Close the TIFF file descriptor
                    XTIFFClose (tif);
 
                    return;
*/
                }
            }
        }
        else {
            fprintf(stderr,"Problem in translating\n");
        }


        if (debug_level & 16)
            fprintf(stderr,"NE neat-line corner = %f\t%f\n",
                    f_NE_x_bounding,
                    f_NE_y_bounding);

        xxx = (double)f_NE_x_bounding;
        yyy = (double)f_NE_y_bounding;

        /* Convert lat/long to projected coordinates */
        if ( GTIFProj4FromLatLong( &defn, 1, &xxx, &yyy ) )
        {
            if (debug_level & 16)
                fprintf(stderr,"%11.3f,%11.3f\n", xxx, yyy);

            /* Convert from PCS coordinates to image pixel coordinates */
            if ( GTIFPCSToImage( gtif, &xxx, &yyy ) )
            {
                if (debug_level & 16)
                    fprintf(stderr,"X/Y Pixels: %f, %f\n", xxx, yyy);

                NE_x = (int)(xxx + 0.5);    /* Tricky way of rounding */
                NE_y = (int)(yyy + 0.5);    /* Tricky way of rounding */

                if (debug_level & 16)
                    fprintf(stderr,"X/Y Pixels: %d, %d\n", NE_x, NE_y);

                if (NE_x < 0 || NE_y < 0 || NE_x >= (int)width || NE_y >= (int)height) {

                    fprintf(stderr,"\nWarning:  NE Neat-line corner calculated at x:%d, y:%d, %s\n",
                        NE_x, NE_y, filenm);
                    fprintf(stderr,"Limits are: 0,0 and %ld,%ld. Resetting corner position.\n",width,height);
                    fprintf(stderr,"Map may appear in the wrong location or scale incorrectly.\n");

                    if (NE_x < 0)
                        NE_x = 0;

                    if (NE_x >= (int)width)
                        NE_x = width - 1;

                    if (NE_y < 0)
                        NE_y = 0;

                    if (NE_y >= (int)height)
                        NE_y = height -1;

/*
                    //Free any memory used and return
                    // We're finished with the geoTIFF key parser, so get rid of it
                    GTIFFree (gtif);

                    // Close the TIFF file descriptor
                    XTIFFClose (tif);
 
                    return;
*/
                }
            }
        }
        else {
            fprintf(stderr,"Problem in translating\n");
        }


        if (debug_level & 16)
            fprintf(stderr,"SW neat-line corner = %f\t%f\n",
                    f_SW_x_bounding,
                    f_SW_y_bounding);

        xxx = (double)f_SW_x_bounding;
        yyy = (double)f_SW_y_bounding;

        /* Convert lat/long to projected coordinates */
        if ( GTIFProj4FromLatLong( &defn, 1, &xxx, &yyy ) )
        {
            if (debug_level & 16)
                fprintf(stderr,"%11.3f,%11.3f\n", xxx, yyy);

            /* Convert from PCS coordinates to image pixel coordinates */
            if ( GTIFPCSToImage( gtif, &xxx, &yyy ) )
            {
                if (debug_level & 16)
                    fprintf(stderr,"X/Y Pixels: %f, %f\n", xxx, yyy);

                SW_x = (int)(xxx + 0.5);    /* Tricky way of rounding */
                SW_y = (int)(yyy + 0.5);    /* Tricky way of rounding */

                if (debug_level & 16)
                    fprintf(stderr,"X/Y Pixels: %d, %d\n", SW_x, SW_y);

                if (SW_x < 0 || SW_y < 0 || SW_x >= (int)width || SW_y >= (int)height) {

                    fprintf(stderr,"\nWarning:  SW Neat-line corner calculated at x:%d, y:%d, %s\n",
                        SW_x, SW_y, filenm);
                    fprintf(stderr,"Limits are: 0,0 and %ld,%ld. Resetting corner position.\n",width,height);
                    fprintf(stderr,"Map may appear in the wrong location or scale incorrectly.\n");

                    if (SW_x < 0)
                        SW_x = 0;

                    if (SW_x >= (int)width)
                        SW_x = width - 1;

                    if (SW_y < 0)
                        SW_y = 0;

                    if (SW_y >= (int)height)
                        SW_y = height -1;

/*
                    //Free any memory used and return
                    // We're finished with the geoTIFF key parser, so get rid of it
                    GTIFFree (gtif);

                    // Close the TIFF file descriptor
                    XTIFFClose (tif);
 
                    return;
*/
                }
            }
        }
        else {
            fprintf(stderr,"Problem in translating\n");
        }


        if (debug_level & 16)
            fprintf(stderr,"SE neat-line corner = %f\t%f\n",
                    f_SE_x_bounding,
                    f_SE_y_bounding);

        xxx = (double)f_SE_x_bounding;
        yyy = (double)f_SE_y_bounding;

        /* Convert lat/long to projected coordinates */
        if ( GTIFProj4FromLatLong( &defn, 1, &xxx, &yyy ) )
        {
            if (debug_level & 16)
                fprintf(stderr,"%11.3f,%11.3f\n", xxx, yyy);

        /* Convert from PCS coordinates to image pixel coordinates */
        if ( GTIFPCSToImage( gtif, &xxx, &yyy ) )
        {
            if (debug_level & 16)
                fprintf(stderr,"X/Y Pixels: %f, %f\n", xxx, yyy);

            SE_x = (int)(xxx + 0.5);    /* Tricky way of rounding */
            SE_y = (int)(yyy + 0.5);    /* Tricky way of rounding */

            if (debug_level & 16)
                fprintf(stderr,"X/Y Pixels: %d, %d\n", SE_x, SE_y);

                if (SE_x < 0 || SE_y < 0 || SE_x >= (int)width || SE_y >= (int)height) {

                    fprintf(stderr,"\nWarning:  SE Neat-line corner calculated at x:%d, y:%d, %s\n",
                        SE_x, SE_y, filenm);
                    fprintf(stderr,"Limits are: 0,0 and %ld,%ld. Resetting corner position.\n",width,height);
                    fprintf(stderr,"Map may appear in the wrong location or scale incorrectly.\n");

                    if (SE_x < 0)
                        SE_x = 0;

                    if (SE_x >= (int)width)
                        SE_x = width - 1;

                    if (SE_y < 0)
                        SE_y = 0;

                    if (SE_y >= (int)height)
                        SE_y = height -1;

/*
                    //Free any memory used and return
                    // We're finished with the geoTIFF key parser, so get rid of it
                    GTIFFree (gtif);

                    // Close the TIFF file descriptor
                    XTIFFClose (tif);
 
                    return;
*/
                }
            }
        }
        else {
            fprintf(stderr,"Problem in translating\n");
        }
    }
    else    /*
             * No map collar to crop off, so we already know
             * where the corner points are.  This is for non-USGS
             * maps.
             */
    {
        NW_x = 0;
        NW_y = 0;

        NE_x = width - 1;
        NE_y = 0;

        SW_x = 0;
        SW_y = height - 1;

        SE_x = width - 1;
        SE_y = height - 1;
    }


    // Here's where we crop off part of the black border for USGS maps.
    if (crop_it)    // USGS maps only
    {
        int i = 3;

        NW_x += i;
        NW_y += i;
        NE_x -= i;
        NE_y += i;
        SW_x += i;      
        SW_y -= i;
        SE_x -= i;
        SE_y -= i;
    }


    // Now figure out the rough pixel crop values from what we know.
    // Image rotation means a simple rectangular crop isn't sufficient.
    if (NW_y < NE_y)
        top_crop = NW_y;
    else
        top_crop = NE_y;


    if (SW_y > SE_y)
        bottom_crop = SW_y;
    else
        bottom_crop = SE_y;

 
    if (NE_x > SE_x)
        right_crop = NE_x;
    else
        right_crop = SE_x;

 
    if (NW_x < SW_x)
        left_crop = NW_x;
    else
        left_crop = SW_x;


    if (!crop_it)       /* If we shouldn't crop the map collar... */
    {
      top_crop = 0;
      bottom_crop = height - 1;
      left_crop = 0;
      right_crop = width - 1;
    }

    // The four crop variables are the maximum rectangle that we
    // wish to keep, rotation notwithstanding (we may want to crop
    // part of some lines due to rotation).  Crop all lines/pixels
    // outside these ranges.

//WE7U
if (top_crop < 0 || top_crop >= (int)height)
top_crop = 0;

if (bottom_crop < 0 || bottom_crop >= (int)height)
bottom_crop = height - 1;

if (left_crop < 0 || left_crop >= (int)width)
left_crop = 0;

if (right_crop < 0 || right_crop >= (int)width)
right_crop = width - 1;

    if (debug_level & 16) {
        fprintf(stderr,"Crop points (pixels):\n");
        fprintf(stderr,"Top: %d\tBottom: %d\tLeft: %d\tRight: %d\n",
        top_crop,
        bottom_crop,
        left_crop,
        right_crop);
    }


    /*
     * The color map is embedded in the geoTIFF file as TIFF tags.
     * We get those tags out of the file and translate to our own
     * colormap.
     * Allocate colors for the map image.  We allow up to 256 colors
     * and allow only 8-bits per pixel in the original map file.  We
     * get our 24-bit RGB colors right out of the map file itself, so
     * the colors should look right.
     * We're picking existing colormap colors that are closest to
     * the original map colors, so we shouldn't run out of colors
     * for other applications.
     *
     * Brightness adjust for the colors?  Implemented in the
     * "geotiff_map_intensity" variable below.
     */

    {
        int l;

        switch (photometric) {
        case PHOTOMETRIC_PALETTE:
            for (l = 0; l < num_colors; l++)
            {
                my_colors[l].red   =   (uint16)(red_orig[l] * geotiff_map_intensity);
                my_colors[l].green = (uint16)(green_orig[l] * geotiff_map_intensity);
                my_colors[l].blue  =  (uint16)(blue_orig[l] * geotiff_map_intensity);

                if (visual_type == NOT_TRUE_NOR_DIRECT)
                    XAllocColor(XtDisplay(w), cmap, &my_colors[l]);
                else
                    pack_pixel_bits(my_colors[l].red, my_colors[l].green, my_colors[l].blue,
                                    &my_colors[l].pixel);
            }
            break;
        case PHOTOMETRIC_MINISBLACK:
            for (l = 0; l < num_colors; l++)
            {
                int v = (l * 255) / (num_colors-1);
                my_colors[l].red = my_colors[l].green = my_colors[l].blue =
                    (uint16)(v * geotiff_map_intensity) << 8;

                if (visual_type == NOT_TRUE_NOR_DIRECT)
                    XAllocColor(XtDisplay(w), cmap, &my_colors[l]);
                else
                    pack_pixel_bits(my_colors[l].red, my_colors[l].green, my_colors[l].blue,
                                    &my_colors[l].pixel);
            }
            break;
        case PHOTOMETRIC_MINISWHITE:
            for (l = 0; l < num_colors; l++)
            {
                int v = (((num_colors-1)-l) * 255) / (num_colors-1);
                my_colors[l].red = my_colors[l].green = my_colors[l].blue =
                  (uint16)(v * geotiff_map_intensity) << 8;

                if (visual_type == NOT_TRUE_NOR_DIRECT)
                    XAllocColor(XtDisplay(w), cmap, &my_colors[l]);
                else
                    pack_pixel_bits(my_colors[l].red, my_colors[l].green, my_colors[l].blue,
                                    &my_colors[l].pixel);
            }
            break;
        }
    }


    // Each data value should be an 8-bit value, which is a
    // pointer into a color
    // table.  Later we perform a translation from the geoTIFF
    // color table to our current color table (matching values
    // as close as possible), at the point where we're writing
    // the image to the pixmap.


    /* We should be ready now to actually read in some
     * pixels and deposit them on the screen.  We will
     * allocate memory for the data area based on the
     * sizes of fields and data in the geoTIFF file.
     */

    bytesPerRow = TIFFScanlineSize(tif);

    if (debug_level & 16) {
        fprintf(stderr,"\nInitial Bytes Per Row: %d\n", bytesPerRow);
    }


    // Here's a tiny malloc that'll hold only one scanline worth of pixels
    imageMemory = (u_char *) malloc(bytesPerRow + 2);
    CHECKMALLOC(imageMemory);


    // TODO:  Figure out the middle boundary on each edge for
    // lat/long and adjust the crop values to match the largest
    // of either the middle or the corners for each edge.  This
    // will help to handle edges that are curved.


    /*
     * There are some optimizations that can still be done:
     *
     * 1) Read in all scanlines but throw away unneeded pixels,
     *    paying attention not to lose smaller details.  Compare
     *    neighboring pixels?
     *
     * 3) Keep a map cache or a screenmap cache to reduce need
     *    for reading map files so often.
     */



    // Here we wish to start at the top line that may have
    // some pixels of interest and proceed to the bottom line
    // of interest.  Process scanlines from top_crop to bottom_crop.
    // Start at the left/right_crop pixels, compute the lat/long
    // of each, using x/y increments so we can quickly scan across
    // the line.
    // Iterate across the line checking whether each pixel is
    // within the viewport.  If so, plot it on the pixmap at
    // the correct scale.

    // Later I may wish to get the lat/lon of each pixel and plot
    // it at the correct point, to handle the curvature of each
    // line (this might be VERY slow).  Right now I treat them as
    // straight lines.

    // At this point we have these variables defined.  The
    // first column contains map corners in Xastir coordinates,
    // the second column contains map corners in pixels:
    //
    // NW corner:
    // NW_x_bounding_wgs84  <-> NW_x
    // NW_y_bounding_wgs84  <-> NW_y
    //
    // NE corner:
    // NE_x_bounding_wgs84  <-> NE_x
    // NE_y_bounding_wgs84  <-> NE_y
    //
    // SW corner:
    // SW_x_bounding_wgs84  <-> SW_x
    // SW_y_bounding_wgs84  <-> SW_y
    //
    // SE corner:
    // SE_x_bounding_wgs84  <-> SE_x
    // SE_y_bounding_wgs84  <-> SE_y

    // I should be able to use these variables to figure out
    // the xastir coordinates of each scanline pixel using
    // linear interpolation along each edge.
    //
    // I don't want to use the crop values in general.  I'd
    // rather crop properly along the neat line instead of a
    // rectangular crop.
    //
    // Define lines along the left/right edges so that I can
    // compute the Xastir coordinates of each pixel along these
    // two lines.  These will be the start/finish of each of my
    // scanlines, and I can use these values to compute the
    // x/y_increment values for each line.  This way I can
    // stretch short lines as I go along, and auto-crop the
    // white border as well.

    // Left_line goes from (top to bottom):
    // NW_x,NW_y -> SW_x,SW_y
    // and from:
    // west_bounding_wgs84,north_bounding_wgs84 -> west_bounding_wgs84,south_bounding_wgs84
    //
    // Right_line goes from(top to bottom):
    // NE_x,NE_y -> SE_x,SE-Y
    // and from:
    // east_bounding_wgs84,north_bounding_wgs84 -> east_bounding_wgs84,south_bounding_wgs84
    //
    // Simpler:  Along each line, Xastir coordinates change how much
    // and in what direction as we move down one scanline?


    // These increments are how much we change in Xastir coordinates and
    // in pixel coordinates as we move down either the left or right
    // neatline one pixel.
    // Be prepared for 0 angle of rotation as well (x-increments = 0).


    // Xastir Coordinate System:
    //
    //              0 (90 deg. or 90N)
    //
    // 0 (-180 deg. or 180W)      129,600,000 (180 deg. or 180E)
    //
    //          64,800,000 (-90 deg. or 90S)


    // Watch out for division by zero here.


    //
    // Left Edge X Increment Per Scanline (Going from top to bottom).
    // This increment will help me to keep track of the left edge of
    // the image, both in Xastir coordinates and in pixel coordinates.
    //
    if (SW_y != NW_y)
    {
        // Xastir coordinates
        xastir_left_x_increment = (float)
            (1.0 * abs(SW_x_bounding_wgs84 - NW_x_bounding_wgs84)   // Need to add one pixel worth here yet
            / abs(SW_y - NW_y));

        // Pixel coordinates
        left_x_increment = (float)(1.0 * abs(SW_x - NW_x)
                            / abs(SW_y - NW_y));

        if (SW_x_bounding_wgs84 < NW_x_bounding_wgs84)
            xastir_left_x_increment = -xastir_left_x_increment;

        if (SW_x < NW_x)
            left_x_increment = -left_x_increment;

//WE7U
//if (abs(left_x_increment) > (width/10)) {
//    left_x_increment = 0.0;
//    xastir_left_x_increment = 0.0;
//}

        if (debug_level & 16)
             fprintf(stderr,"xastir_left_x_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
             xastir_left_x_increment,
             SW_x_bounding_wgs84,
             NW_x_bounding_wgs84,
             left_x_increment,
             SW_x,
             NW_x,
             bottom_crop,
             top_crop);
    }
    else
    {
        // Xastir coordinates
        xastir_left_x_increment = 0;

        // Pixel coordinates
        left_x_increment = 0;
    }


    //
    // Left Edge Y Increment Per Scanline (Going from top to bottom)
    // This increment will help me to keep track of the left edge of
    // the image, both in Xastir coordinates and in pixel coordinates.
    //
    if (SW_y != NW_y)
    {
        // Xastir coordinates
        xastir_left_y_increment = (float)
            (1.0 * abs(SW_y_bounding_wgs84 - NW_y_bounding_wgs84)   // Need to add one pixel worth here yet
            / abs(SW_y - NW_y));

        // Pixel coordinates
        left_y_increment = (float)1.0; // Aren't we going down one pixel each time?

        if (SW_y_bounding_wgs84 < NW_y_bounding_wgs84)  // Ain't gonn'a happen
            xastir_left_y_increment = -xastir_left_y_increment;

//WE7U
//if (abs(left_y_increment) > (width/10)) {
//    xastir_left_y_increment = 0.0;
//}

        if (debug_level & 16)
             fprintf(stderr,"xastir_left_y_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
             xastir_left_y_increment,
             SW_y_bounding_wgs84,
             NW_y_bounding_wgs84,
             left_y_increment,
             SW_y,
             NW_y,
             bottom_crop,
             top_crop);
    }
    else
    {
        // Xastir coordinates
        xastir_left_y_increment = 0;

        // Pixel coordinates
        left_y_increment = 0;
    }


    //
    // Right Edge X Increment Per Scanline (Going from top to bottom)
    // This increment will help me to keep track of the right edge of
    // the image, both in Xastir coordinates and image coordinates.
    //
    if (SE_y != NE_y)
    {
        // Xastir coordinates
        xastir_right_x_increment = (float)
            (1.0 * abs(SE_x_bounding_wgs84 - NE_x_bounding_wgs84)   // Need to add one pixel worth here yet
            / abs(SE_y - NE_y));

        // Pixel coordinates
        right_x_increment = (float)(1.0 * abs(SE_x - NE_x)
                            / abs(SE_y - NE_y));

        if (SE_x_bounding_wgs84 < NE_x_bounding_wgs84)
            xastir_right_x_increment = -xastir_right_x_increment;

        if (SE_x < NE_x)
            right_x_increment = -right_x_increment;

//WE7U
//if (abs(right_x_increment) > (width/10)) {
//    right_x_increment = 0.0;
//    xastir_right_x_increment = 0.0;
//}

        if (debug_level & 16)
            fprintf(stderr,"xastir_right_x_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
            xastir_right_x_increment,
            SE_x_bounding_wgs84,
            NE_x_bounding_wgs84,
            right_x_increment,
            SE_x,
            NE_x,
            bottom_crop,
            top_crop);
    }
    else
    {
        // Xastir coordinates
        xastir_right_x_increment = 0;

        // Pixel coordinates
        right_x_increment = 0;
    }


    //
    // Right Edge Y Increment Per Scanline (Going from top to bottom)
    // This increment will help me to keep track of the right edge of
    // the image, both in Xastir coordinates and in image coordinates.
    //
    if (SE_y != NE_y)
    {
        // Xastir coordinates
        xastir_right_y_increment = (float)
            (1.0 * abs(SE_y_bounding_wgs84 - NE_y_bounding_wgs84)   // Need to add one pixel worth here yet
            / abs(SE_y - NE_y));

        // Pixel coordinates
        right_y_increment = (float)1.0;    // Aren't we going down one pixel each time?

        if (SE_y_bounding_wgs84 < NE_y_bounding_wgs84)  // Ain't gonn'a happen
            xastir_right_y_increment = -xastir_right_y_increment;

//WE7U
//if (abs(right_y_increment) > (width/10)) {
//    xastir_right_y_increment = 0.0;
//}

        if (debug_level & 16)
            fprintf(stderr,"xastir_right_y_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
            xastir_right_y_increment,
            SE_y_bounding_wgs84,
            NE_y_bounding_wgs84,
            right_y_increment,
            SE_y,
            NE_y,
            bottom_crop,
            top_crop);
    }
    else
    {
        // Xastir coordinates
        xastir_right_y_increment = 0;

        // Pixel coordinates
        right_y_increment = 0;
    }


    if (debug_level & 16) {
        fprintf(stderr," Left x increments: %f %f\n", xastir_left_x_increment, left_x_increment);
        fprintf(stderr," Left y increments: %f %f\n", xastir_left_y_increment, left_y_increment);
        fprintf(stderr,"Right x increments: %f %f\n", xastir_right_x_increment, right_x_increment);
        fprintf(stderr,"Right y increments: %f %f\n", xastir_right_y_increment, right_y_increment);
    }


    // Compute how much "y" changes per pixel as we traverse from left to right
    // along a scanline along the top of the image.
    //
    // Top Edge Y Increment Per X-Pixel Width (Going from left to right).
    // This increment will help me to get rid of image rotation.
    //
    if (NE_x != NW_x)
    {
        // Xastir coordinates
        xastir_top_y_increment = (float)
            (1.0 * abs(NE_y_bounding_wgs84 - NW_y_bounding_wgs84)   // Need to add one pixel worth here yet
            / abs(NE_x - NW_x));    // And a "+ 1.0" here?

        // Pixel coordinates
        top_y_increment = (float)(1.0 * abs(NE_y - NW_y)
                    / abs(NE_x - NW_x));

        if (NE_y_bounding_wgs84 < NW_y_bounding_wgs84)
            xastir_top_y_increment = -xastir_top_y_increment;

        if (NE_y < NW_y)
            top_y_increment = -top_y_increment;

        if (debug_level & 16)
            fprintf(stderr,"xastir_top_y_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
            xastir_top_y_increment,
            NE_y_bounding_wgs84,
            NW_y_bounding_wgs84,
            top_y_increment,
            NE_y,
            NW_y,
            right_crop,
            left_crop);
    }
    else
    {
        // Xastir coordinates
        xastir_top_y_increment = 0;

        // Pixel coordinates
        top_y_increment = 0;
    }


    // Compute how much "y" changes per pixel as you traverse from left to right
    // along a scanline along the bottom of the image.
    //
    // Bottom Edge Y Increment Per X-Pixel Width (Going from left to right).
    // This increment will help me to get rid of image rotation.
    //
    if (SE_x != SW_x)
    {
        // Xastir coordinates
        xastir_bottom_y_increment = (float)
            (1.0 * abs(SE_y_bounding_wgs84 - SW_y_bounding_wgs84)   // Need to add one pixel worth here yet
            / abs(SE_x - SW_x));    // And a "+ 1.0" here?

        // Pixel coordinates
        bottom_y_increment = (float)(1.0 * abs(SE_y - SW_y)
                        / abs(SE_x - SW_x));

        if (SE_y_bounding_wgs84 < SW_y_bounding_wgs84)  
            xastir_bottom_y_increment = -xastir_bottom_y_increment;

        if (SE_y < SW_y)
            bottom_y_increment = -bottom_y_increment;

        if (debug_level & 16)
            fprintf(stderr,"xastir_bottom_y_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
            xastir_bottom_y_increment,
            SE_y_bounding_wgs84,
            SW_y_bounding_wgs84,
            bottom_y_increment,
            SE_y,
            SW_y,
            right_crop,
            left_crop);
    }
    else
    {
        // Xastir coordinates
        xastir_bottom_y_increment = 0;

        // Pixel coordinates
        bottom_y_increment = 0;
    }


    // Find the average change in Y as we traverse from left to right one pixel
    xastir_avg_y_increment = (float)(xastir_top_y_increment + xastir_bottom_y_increment) / 2.0;
    avg_y_increment = (float)(top_y_increment + bottom_y_increment) / 2.0;


    // Find edges of current viewport in Xastir coordinates
    //
    view_min_x = x_long_offset;                         /*   left edge of view */
    if (view_min_x > 129600000l)
        view_min_x = 0;

    view_max_x = x_long_offset + (screen_width * scale_x); /*  right edge of view */
    if (view_max_x > 129600000l)
        view_max_x = 129600000l;

    view_min_y = y_lat_offset;                          /*    top edge of view */
    if (view_min_y > 64800000l)
        view_min_y = 0;

    view_max_y = y_lat_offset + (screen_height * scale_y); /* bottom edge of view */
    if (view_max_y > 64800000l)
        view_max_y = 64800000l;


    /* Get the pixel scale */
    have_PixelScale = TIFFGetField( tif, TIFFTAG_GEOPIXELSCALE, &qty, &PixelScale );
    if (debug_level & 16) {
        if (have_PixelScale) {
            fprintf(stderr,"PixelScale: %f %f %f\n",
                *PixelScale,
                *(PixelScale + 1),
                *(PixelScale + 2) );
        }
        else {
            fprintf(stderr,"No PixelScale tag found in file\n");
        }
    }


    // Use PixelScale to determine lines to skip at each
    // zoom level?
    // O-size map:
    // ModelPixelScaleTag (1,3):
    //   2.4384           2.4384           0
    //
    // F-size map:
    // ModelPixelScaleTag (1,3):
    //   10.16            10.16            0                
    //
    // C-size map:
    // ModelPixelScaleTag (1,3):
    //   25.400001        25.400001        0                
    //

    if (debug_level & 16)
        fprintf(stderr,"Size: x %ld, y %ld\n", scale_x,scale_y);


    // I tried to be very aggressive with the scaling factor
    // below (3.15) in order to skip the most possible rows
    // to speed things up.  If you see diagonal lines across
    // the maps, increase this number (up to a max of 4.0
    // probably).  A higher number means less rows skipped,
    // which improves the look but slows the map drawing down.
    //
    if (have_PixelScale) {
        SkipRows = (int)( ( scale_y / ( *PixelScale * 3.15 ) ) + 0.5 );
        if (SkipRows < 1)
            SkipRows = 1;
        if (SkipRows > (int)(height / 10) )
            SkipRows = height / 10;
    }
    else {
        SkipRows = 1;
    }
    if (debug_level & 16)
        fprintf(stderr,"SkipRows: %d\n", SkipRows);

    // Use SkipRows to set increments for the loops below.


    if ( top_crop <= 0 )
        top_crop = 0;               // First row of image

    if ( ( bottom_crop + 1) >= (int)height )
        bottom_crop = height - 1;   // Last row of image

    // Here I pre-compute some of the values I'll need in the
    // loops below in order to save some time.
    NW_line_offset = (int)(NW_y - top_crop);
    NE_line_offset = (int)(NE_y - top_crop);

    NW_xastir_x_offset =  (int)(xastir_left_x_increment * NW_line_offset);
    NE_xastir_x_offset = (int)(xastir_right_x_increment * NE_line_offset);
    NW_xastir_y_offset =  (int)(xastir_left_y_increment * NW_line_offset);

    NW_x_offset =  (int)(1.0 * left_x_increment * NW_line_offset);
    NE_x_offset = (int)(1.0 * right_x_increment * NE_line_offset);
    xastir_avg_left_right_y_increment = (float)((xastir_right_y_increment + xastir_left_y_increment) / 2.0);
    total_avg_y_increment = (float)(xastir_avg_left_right_y_increment * avg_y_increment);


    // (Xastir bottom - Xastir top) / height
    //steph = (double)( (left_y_increment + right_y_increment) / 2); 
    // NOTE:  This one does not take into account current height
    steph = (float)( (SW_y_bounding_wgs84 - NW_y_bounding_wgs84)
                      / (1.0 * (SW_y - NW_y) ) );

    // Compute scaled pixel size for XFillRectangle
    stephc = (int)( ( (1.50 * steph / scale_x) + 1.0) + 1.5);

    view_top_minus_pixel_height = (unsigned long)(view_min_y - steph);

    // Iterate over the rows of interest only.  Using the rectangular
    // top/bottom crop values for these is ok at this point.
    //
    // Put row multipliers above loops.  Try to get as many
    // multiplications as possible outside the loops.  Adds and
    // subtracts are ok.  Try to do as little floating point stuff
    // as possible inside the loops.  I also declared a lot of
    // the inner loop stuff as register variables.  Saved me about
    // a second per map (not much, but I'll take what I can get!)
    //
    for ( row = top_crop; (int)row < bottom_crop + 1; row+= SkipRows )
    {
        int skip = 0;

        // Our offset from the top row of the map neatline
        // (kind of... ignoring rotation anyway).
        row_offset = row - top_crop;
        //fprintf(stderr,"row_offset: %d\n", row_offset);


        // Compute the line end-points in Xastir coordinates
        // Initially was a problem here:  Offsetting from NW_x_bounding but
        // starting at top_crop line.  Fixed by last term added to two
        // equations below.

        current_xastir_left = (unsigned long)
              ( NW_x_bounding_wgs84
            + ( 1.0 * xastir_left_x_increment * row_offset )
            -   NW_xastir_x_offset );

        current_xastir_right = (unsigned long)
              ( NE_x_bounding_wgs84
            + ( 1.0 * xastir_right_x_increment * row_offset )
            -   NE_xastir_x_offset );


        //if (debug_level & 16)
        //  fprintf(stderr,"Left: %ld  Right:  %ld\n",
        //      current_xastir_left,
        //      current_xastir_right);


        // In pixel coordinates:
        current_left = (int)
                          ( NW_x
                        + ( 1.0 * left_x_increment * row_offset ) + 0.5
                        -   NW_x_offset );

        current_right = (int)
                          ( NE_x
                        + ( 1.0 * right_x_increment * row_offset ) + 0.5
                        -   NE_x_offset );

//WE7U:  This comparison is always false:  current_left is unsigned
//therefore always positive!
if (current_left < 0)
    current_left = 0;

if (current_right >= width)
    current_right = width - 1;

        current_line_width = current_right - current_left + 1;  // Pixels


        // if (debug_level & 16)
        //     fprintf(stderr,"Left: %ld  Right: %ld  Width: %ld\n",
        //         current_left,
        //         current_right, current_line_width);


        // Compute original pixel size in Xastir coordinates.  Note
        // that this can change for each scanline in a USGS geoTIFF.

        // (Xastir right - Xastir left) / width-of-line
        // Need the "1.0 *" or the math will be incorrect (won't be a float)
        stepw = (float)( (current_xastir_right - current_xastir_left)
                      / (1.0 * (current_right - current_left) ) );



        // if (debug_level & 16)
        //     fprintf(stderr,"\t\t\t\t\t\tPixel Width: %f\n",stepw);

        // Compute scaled pixel size for XFillRectangle
        stepwc = (int)( ( (1.0 * stepw / scale_x) + 1.0) + 0.5);


        // In Xastir coordinates
        xastir_current_y = (unsigned long)(NW_y_bounding_wgs84
                  + (xastir_left_y_increment * row_offset) );

        xastir_current_y = (unsigned long)(xastir_current_y - NW_xastir_y_offset);


        view_left_minus_pixel_width = view_min_x - stepw;


        // Check whether any part of the scanline will be within the
        // view.  If so, read the scanline from the file and iterate
        // across the pixels.  If not, skip this line altogether.

        // Compute right edge of image
        xastir_total_y = (unsigned long)
                           ( xastir_current_y
                         - ( total_avg_y_increment * (current_right - current_left) ) );

        // Check left edge y-value then right edge y-value.
        // If either are within view, process the line, else skip it.
        if ( ( ( xastir_current_y <= view_max_y) && (xastir_total_y >= view_top_minus_pixel_height) )
            || ( ( xastir_total_y <= view_max_y ) && ( xastir_total_y >= view_top_minus_pixel_height ) ) )
        {
            // Read one geoTIFF scanline
            if (TIFFReadScanline(tif, imageMemory, row, 0) < 0)
                break;  // No more lines to read or we couldn't read the file at all


            // Iterate over the columns of interest, skipping the left/right
            // cropped pixels, looking for pixels that fit within our viewport.
            //
            for ( column = current_left; column < (current_right + 1); column++ )
            {
                skip = 0;


                column_offset = column - current_left;  // Pixels

                //fprintf(stderr,"Column Offset: %ld\n", column_offset);  // Pixels
                //fprintf(stderr,"Current Left: %ld\n", current_left);    // Pixels

                xastir_current_x = (unsigned long)
                                    current_xastir_left
                                    + (stepw * column_offset);    // In Xastir coordinates

                // Left line y value minus
                // avg y-increment per scanline * avg y-increment per x-pixel * column_offset
                xastir_total_y = (unsigned long)
                                  ( xastir_current_y
                                - ( total_avg_y_increment * column_offset ) );

                //fprintf(stderr,"Xastir current: %ld %ld\n", xastir_current_x, xastir_current_y);


                // Check whether pixel fits within boundary lines (USGS maps)
                // This is how we get rid of the last bit of white border at
                // the top and bottom of the image.
                if (have_fgd)   // USGS map
                {
                    if (   (xastir_total_y > SW_y_bounding_wgs84)
                        || (xastir_total_y < NW_y_bounding_wgs84) )
                    skip++;


                    // Here's a trick to make it look like the map pages join better.
                    // If we're within a certain distance of a border, change any black
                    // pixels to white (changes map border to less obtrusive color).
                    if ( *(imageMemory + column) == 0x00 )  // If pixel is Black
                    {
                        if ( (xastir_total_y > (SW_y_bounding_wgs84 - 25) )
                            || (xastir_total_y < (NW_y_bounding_wgs84 + 25) )
                            || (xastir_current_x < (SW_x_bounding_wgs84 + 25) )
                            || (xastir_current_x > (SE_x_bounding_wgs84 - 25) ) )
                        {
//WE7U: column is unsigned so "column >= 0" is always true
                            if ((int)column < bytesPerRow && column >= 0) {
                                *(imageMemory + column) = 0x01; // Change to White
                            }
                            else {
//WE7U
//                                fprintf(stderr,"draw_geotiff_image_map: Bad fgd file for map?: %s\n", filenm);
                            }
                        }
                    }
                }


                /* Look for left or right map boundaries inside view */    
                if ( !skip
                    && ( xastir_current_x <= view_max_x )
                    && ( xastir_current_x >= view_left_minus_pixel_width )    
                    && ( xastir_total_y <= view_max_y )
                    && ( xastir_total_y >= view_top_minus_pixel_height ) )
                {
                    // Here are the corners of our viewport, using the Xastir
                    // coordinate system.  Notice that Y is upside down:
                    // 
                    // left edge of view = x_long_offset
                    // right edge of view = x_long_offset + (screen_width  * scale_x)
                    // top edge of view =  y_lat_offset
                    // bottom edge of view =  y_lat_offset + (screen_height * scale_y)


                    // Compute the screen position of the pixel and scale it
                    sxx = (xastir_current_x - x_long_offset) / scale_x;
                    syy = (xastir_total_y   - y_lat_offset ) / scale_y;

                    // Set the color for the pixel
                    XSetForeground (XtDisplay (w), gc, my_colors[*(imageMemory + column)].pixel);

                    // And draw the pixel
                    XFillRectangle (XtDisplay (w), pixmap, gc, sxx, syy, stepwc, stephc);
                }
            }
        }
    }


    /* Free up any malloc's that we did */
    if (imageMemory)
        free(imageMemory);


    if (debug_level & 16)
        fprintf(stderr,"%d rows read in\n", (int) row);

    /* We're finished with the geoTIFF key parser, so get rid of it */
    GTIFFree (gtif);

    /* Close the TIFF file descriptor */
    XTIFFClose (tif);

    // Close the filehandles that are left open after the
    // four GTIFImageToPCS calls.
    //(void)CSVDeaccess(NULL);
}
#endif /* HAVE_LIBGEOTIFF */





// Test map visibility (on screen)
//
// Input parameters are in Xastir coordinate system (fastest for us)
//
// Returns: 0 if map is _not_ visible
//          1 if map _is_ visible
//
static int map_onscreen(long left, long right, long top, long bottom) {
    unsigned long max_x_long_offset;
    unsigned long max_y_lat_offset;
    long map_border_min_x;
    long map_border_max_x;
    long map_border_min_y;
    long map_border_max_y;
    long x_test, y_test;
    int in_window = 0;

    max_x_long_offset=(unsigned long)(x_long_offset+ (screen_width * scale_x));
    max_y_lat_offset =(unsigned long)(y_lat_offset + (screen_height* scale_y));

    if (debug_level & 16)
      fprintf(stderr,"x_long_offset: %ld, y_lat_offset: %ld, max_x_long_offset: %ld, max_y_lat_offset: %ld\n",
             x_long_offset, y_lat_offset, (long)max_x_long_offset, (long)max_y_lat_offset);

    if (((left <= x_long_offset) && (x_long_offset <= right) &&
         (top <= y_lat_offset) && (y_lat_offset <= bottom)) ||
        ((left <= x_long_offset) && (x_long_offset <= right) &&
         (top <= (long)max_y_lat_offset) && ((long)max_y_lat_offset <= bottom)) ||
        ((left <= (long)max_x_long_offset) && ((long)max_x_long_offset <= right) &&
         (top <= y_lat_offset) && (y_lat_offset <= bottom)) ||
        ((left <= (long)max_x_long_offset) && ((long)max_x_long_offset <= right) &&
         (top <= (long)max_y_lat_offset) && ((long)max_y_lat_offset <= bottom)) ||
        ((x_long_offset <= left) && (left <= (long)max_x_long_offset) &&
         (y_lat_offset <= top) && (top <= (long)max_y_lat_offset)) ||
        ((x_long_offset <= left) && (left <= (long)max_x_long_offset) &&
         (y_lat_offset <= bottom) && (bottom <= (long)max_y_lat_offset)) ||
        ((x_long_offset <= right) && (right <= (long)max_x_long_offset) &&
         (y_lat_offset <= top) && (top <= (long)max_y_lat_offset)) ||
        ((x_long_offset <= right) && (right <= (long)max_x_long_offset) &&
         (y_lat_offset <= bottom) && (bottom <= (long)max_y_lat_offset)))
      in_window = 1;
    else {
        // find min and max borders to look at
        //this routine are for those odd sized maps
        if ((long)left > x_long_offset)
          map_border_min_x = (long)left;
        else
          map_border_min_x = x_long_offset;

        if (right < (long)max_x_long_offset)
          map_border_max_x = (long)right;
        else
          map_border_max_x = (long)max_x_long_offset;

        if ((long)top > y_lat_offset)
          map_border_min_y = (long)top;
        else
          map_border_min_y = y_lat_offset;

        if (bottom < (long)max_y_lat_offset)
          map_border_max_y = (long)bottom;
        else
          map_border_max_y = (long)max_y_lat_offset;

        // do difficult check inside map
        for (x_test = map_border_min_x;(x_test <= map_border_max_x && !in_window); x_test += ((scale_x * screen_width) / 10)) {
            for (y_test = map_border_min_y;(y_test <= map_border_max_y && !in_window);y_test += ((scale_y * screen_height) / 10)) {
                if ((x_long_offset <= x_test) && (x_test <= (long)max_x_long_offset) && (y_lat_offset <= y_test) &&
                    (y_test <= (long)max_y_lat_offset))

                  in_window = 1;
            }
        }
    }
    return (in_window);
}





// Function which checks whether a map is onscreen, but does so by
// finding the map boundaries from the map index.  The only input
// parameter is the complete path/filename.
//
// Returns: 0 if map is _not_ visible
//          1 if map _is_ visible
//          2 if the map is not in the index
//
static int map_onscreen_index(char *filename) {
    unsigned long top, bottom, left, right;
    int onscreen = 2;
    int map_layer, draw_filled, auto_maps;     // Unused in this function


    if (index_retrieve(filename, &bottom, &top, &left, &right,
            &map_layer, &draw_filled, &auto_maps) ) {

        // Map was in the index, check for visibility
        if (map_onscreen(left, right, top, bottom)) {
            // Map is visible
            onscreen = 1;
            //fprintf(stderr,"Map found in index and onscreen! %s\n",filename);
        }
        else {  // Map is not visible
            onscreen = 0;
            //fprintf(stderr,"Map found in index but not onscreen: %s\n",filename);
        }
    }
    else {  // Map is not in the index
        onscreen = 2;
        //fprintf(stderr,"Map not found in index: %s\n",filename);
    }
    return(onscreen);
}
 




// Note:  There are long's in the pdb_hdr that are not being used.
// If they were, the ntohl() function would need to be used in order
// to make sure they were represented correctly on big-endian and
// little-endian machines.  Same goes for short's, make sure ntohs()
// is used in all cases.
//
// Also Note: It looks like all the values in the data structures are 
// unsigned, we may have to explicitly define the long's as well.  -KD6ZWR
//
void draw_palm_image_map(Widget w, char *dir, char *filenm,
        int destination_pixmap, int draw_filled) {

// Do NOT change any of these structs.  They have to match the
// structs  that the palm maps were made with.
#pragma pack(1)
    struct {
        char name[32];
        short file_attributes;
        short version;  // 2: No placeholder bytes in file, 3: w/bytes
        long creation_date;
        long modification_date;
        long backup_date;
        long modification_number;
        long app_info;
        long sort_info;
        char database_type[4];
        char creator_type[4];
        long unique_id_seed;
        long next_record_list;
        short number_of_records;
    } pdb_hdr;

    struct {
        long record_data_offset;
        char category;
        char id[3];
    } prl;

    // Two placeholder bytes where were added in version 3.  We
    // skip these below automatically using the fseek calls and the
    // record offsets.
    //struct {
    //    short placeholder;
    //} pdb_ph;

    struct {
        long left_bounds;
        long right_bounds;
        long top_bounds;
        long bottom_bounds;
        char menu_name[12];
        short granularity;
        char sort_order;
        char fill[33];
    } pmf_hdr;

    struct {
        char type;
        char sub_type;
        short minimum_zoom;
    } record_hdr;

    struct {
        unsigned short next_vector;
        unsigned short left_bounds;
        unsigned short right_bounds;
        unsigned short top_bounds;
        unsigned short bottom_bounds;
        unsigned short line_start_x;
        unsigned short line_start_y;
    } vector_hdr;

    struct {
        unsigned char next_x;
        unsigned char next_y;
    } vector_point;

    struct {
        unsigned short next_label;
        unsigned short start_x;
        unsigned short start_y;
        char symbol_set;
        char symbol_char;
        char color;
        char treatment;
        short fill;
        char text[20];
    } label_record;     

    FILE *fn;
    char filename[MAX_FILENAME];
    int records, record_count, count;
    int scale;
    long map_left, map_right, map_top, map_bottom, max_x, max_y;
    long record_ptr;
    long line_x, line_y;
    int vector;
    char status_text[MAX_FILENAME];


    xastir_snprintf(filename, sizeof(filename), "%s/%s", dir, filenm);

    if ((fn = fopen(filename, "r")) != NULL) {

        if (debug_level & 16)
            fprintf(stderr,"opened file: %s\n", filename);

        fread(&pdb_hdr, sizeof(pdb_hdr), 1, fn);

        if (strncmp(pdb_hdr.database_type, "map1", 4) != 0
                || strncmp(pdb_hdr.creator_type, "pAPR", 4) != 0) {
            fprintf(stderr,"Not Palm OS Map: %s\n", filename);
            fclose(fn);
            return;
        }

        records = ntohs(pdb_hdr.number_of_records);

        fread(&prl, sizeof(prl), 1, fn);

        if (debug_level & 512) {
            fprintf(stderr,"Palm Map: %s, %d records, offset: %8x\n",
                pdb_hdr.name,
                records,
                (unsigned int)ntohl(prl.record_data_offset));
        }

        // Save the current pointer into the file
        record_ptr = (long)ftell(fn);
        // record_ptr should now be point to the next record list in
        // the sequence (if there is another one).

        // Point to the map file header corresponding to the record
        // list we just read in & snag it.
        fseek(fn, ntohl(prl.record_data_offset), SEEK_SET);
        fread(&pmf_hdr, sizeof(pmf_hdr), 1, fn);

        scale = ntohs(pmf_hdr.granularity);
        map_left = ntohl(pmf_hdr.left_bounds);
        map_right = ntohl(pmf_hdr.right_bounds);
        map_top = ntohl(pmf_hdr.top_bounds);
        map_bottom = ntohl(pmf_hdr.bottom_bounds);

        if (debug_level & 512) {
            fprintf(stderr,"\tLeft %ld, Right %ld, Top %ld, Bottom %ld, %s, Scale %d, %d\n",
                map_left,
                map_right,
                map_top,
                map_bottom,
                pmf_hdr.menu_name,
                scale,
                pmf_hdr.sort_order);
        }

        // DNN: multipy by 10; pocketAPRS corners in tenths of seconds,
        // internal map in hundredths of seconds (was "scale" which was wrong,
        // scale is not used for the map corners)
        // Multipy now so we don't have to do it for every use below...
        map_left = map_left * 10;
        map_right = map_right * 10;
        map_top = map_top * 10;
        map_bottom = map_bottom * 10;


        // Check whether we're indexing or drawing the map
        if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
                || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {

            // We're indexing only.  Save the extents in the index.
            index_update_xastir(filenm,     // Filename only
                map_bottom, // Bottom
                map_top,    // Top
                map_left,   // Left
                map_right); // Right

            fclose(fn);
            return; // Done indexing this file
        }


        if (map_onscreen(map_left, map_right, map_top, map_bottom)) {


            // Update the statusline for this map name
            // Check whether we're indexing or drawing the map
            if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
                    || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {
                xastir_snprintf(status_text, sizeof(status_text), langcode ("BBARSTA039"), filenm);
            }
            else {
                xastir_snprintf(status_text, sizeof(status_text), langcode ("BBARSTA028"), filenm);
            }
            statusline(status_text,0);       // Loading/Indexing ...


            max_x = (long)(screen_width + MAX_OUTBOUND);
            max_y = (long)(screen_height + MAX_OUTBOUND);

            /* read vectors */
            for (record_count = 2; record_count <= records; record_count++) {

                // Point to the next record list header & snag it
                fseek(fn, record_ptr, SEEK_SET);
                fread(&prl, sizeof(prl), 1, fn);

                if (debug_level & 512) {
                    fprintf(stderr,"\tRecord %d, offset: %8x\n",
                        record_count,
                        (unsigned int)ntohl(prl.record_data_offset));
                }

                // Save a pointer to the next record list header
                record_ptr = (long)ftell(fn);

                // Point to the next map file header & snag it
                fseek(fn, ntohl(prl.record_data_offset), SEEK_SET);
                fread(&record_hdr, sizeof(record_hdr), 1, fn);

                if (debug_level & 512) {
                    fprintf(stderr,"\tType %d, Sub %d, Zoom %d\n",
                        record_hdr.type,
                        record_hdr.sub_type,
                        (unsigned short) ntohs(record_hdr.minimum_zoom));
                }

                if (record_hdr.type > 0 && record_hdr.type < 16) {

                    vector = True;

                    while (vector && fread(&vector_hdr, sizeof(vector_hdr), 1, fn)) {

                        count = (unsigned short) ntohs(vector_hdr.next_vector);

                        if (count && !(count&1)) {
                            line_x = (long)ntohs(vector_hdr.line_start_x);
                            line_y = (long)ntohs(vector_hdr.line_start_y);

                            if (debug_level & 512) {
                                fprintf(stderr,"\tvector %d, left %d, right %d, top %d, bottom %d, start x %ld, start y %ld\n",
                                    count,
                                    (unsigned short) ntohs(vector_hdr.left_bounds),
                                    (unsigned short) ntohs(vector_hdr.right_bounds),
                                    (unsigned short) ntohs(vector_hdr.top_bounds),
                                    (unsigned short) ntohs(vector_hdr.bottom_bounds),
                                    line_x,
                                    line_y);
                            }

                            // DNN: Only line_x and line_y are scaled,
                            // not map_left and map_top
                            map_plot (w,
                                max_x,
                                max_y,
                                map_left + (line_x * scale),
                                map_top + (line_y * scale),
                                record_hdr.type,    // becomes the color choice
                                0,
                                destination_pixmap,
                                draw_filled);

                            for (count -= sizeof(vector_hdr); count > 0; count -= sizeof(vector_point)) {

                                fread(&vector_point, sizeof(vector_point), 1, fn);

                                if (debug_level & 512) {
                                    fprintf(stderr,"\tnext x %d, next y %d\n",
                                        vector_point.next_x,
                                        vector_point.next_y);
                                }

                                line_x += vector_point.next_x - 127;
                                line_y += vector_point.next_y - 127;

                                // DNN: Only line_x and line_y are scaled,
                                // not map_left and map_top
                                map_plot (w,
                                    max_x,
                                    max_y,
                                    map_left + (line_x * scale),
                                    map_top + (line_y * scale),
                                    record_hdr.type,
                                    0,
                                    destination_pixmap,
                                    draw_filled);
                            }

                            // DNN: Only line_x and line_y are scaled,
                            // not map_left and map_top
                            map_plot (w,
                                max_x,
                                max_y,
                                map_left + (line_x * scale),
                                map_top + (line_y * scale),
                                0,  // Color 0
                                0,
                                destination_pixmap,
                                draw_filled);
                        }
                        else {
                            vector = False;
                        }
                    }
                }
                else if ( (record_hdr.type == 0)    // We have a label
                        && map_labels) {  // and we wish to draw it
                    long label_x_cord;
                    long label_y_cord;
                    int  label_length;
                    int  label;
                    long  label_mag;
                    long x;
                    long y;
                    int  i;
                    int color;


                    label_mag = (long) ntohs(record_hdr.minimum_zoom);
 
                    // DNN: Multiplication by 4 looks reasonable on the map I
                    // checked, be my guest to come up with a better value...
                    // For the map I used the behaviour mimics the
                    // behaviour on pocketAPRS when the labels show up
                    label_mag *= 4;
 
                    label = True;

                    while (label && fread(&label_record, sizeof(label_record), 1, fn)) {

                        count = ntohs(label_record.next_label);

                        if (count && !(count&1)) {
                            line_x = (long)ntohs(vector_hdr.line_start_x);
                            line_y = (long)ntohs(vector_hdr.line_start_y);
 
                            if (debug_level & 512) {
                                fprintf(stderr,"\t%d, %d, %d, %d, %d, %d, 0x%x, %s\n",
                                    ntohs(label_record.next_label),
                                    ntohs(label_record.start_x),
                                    ntohs(label_record.start_y),
                                    label_record.symbol_set,
                                    label_record.symbol_char,
                                    label_record.color,
                                    label_record.treatment,
                                    label_record.text);
                            }
 
                            label_x_cord = map_left +
                                (((long) ntohs(label_record.start_x)) * scale);
                            label_y_cord = map_top +
                                (((long) ntohs(label_record.start_y)) * scale);
 
                            // DNN:  Skip empty labels
                            if(label_record.text[0] != '\0') {
                                label_record.text[19] = '\0';  // Make sure we have a terminator
                                label_length = (int)strlen (label_record.text);
 
                                for (i = (label_length - 1); i > 0; i--) {
                                    if (label_record.text[i] == ' ')
                                        label_record.text[i] = '\0';
                                    else
                                        break;
                                }
 
                                label_length = (int)strlen (label_record.text);
               
                                // DNN: todo: treatment:
                                // bit 7: inverse
                                // bit 6: draw beneath map line data
                                // bit 5,4: 00 = left, 01 = center, 10 = right
                                // bit 3,2,1,0: typeface 0 = normal, 1 = bold,
                                //        2 = large, 3 = extra large
                                //
                                // For now KISS, just put it on the map.
 
                                x = ((label_x_cord - x_long_offset) / scale_x);
 
                                /* examine bits 4 and 5 of treatment */
                                if((label_record.treatment & 0x30) == 0x00) {
                                    /* left of coords */
                                    x = x - (label_length * 6);
                                }
                                else if((label_record.treatment & 0x30) == 0x10) {
                                    /* center */
                                    x = x - (label_length * 3);
                                }
                                else {
                                    /* right of coords */
                                    x = ((label_x_cord - x_long_offset) / scale_x);
                                }
 
                                y = ((label_y_cord - y_lat_offset) / scale_y);
 
                                // Color selection
                                switch (label_record.color) {
                                    case 0:
                                        color = 0x08;   // black
                                        break;
                                    case 1:
                                        color = 0x07;   // darkgray
                                        break;
                                    case 2:
                                        color = 0x14;   // lightgray
                                        break;
                                    case 3:
                                        color = 0x0f;   // white
                                        break;
                                    default:
                                        color = 0x08;   // black
                                        break;
                                }

                                if (label_mag > (int)((scale_x) - 1) || label_mag == 0) {
                                    if (x > (0) && (x < (int)screen_width)) {
                                        if (y > (0) && (y < (int)screen_height)) {
                                            draw_rotated_label_text (w,
                                                -90.0,
                                                x,
                                                y,
                                                label_length,
                                                colors[color],
                                                label_record.text);
                                        }
                                    }
                                }
                            }
                            /* Label has a symbol */
                            if(label_record.symbol_char != '\0') {
                                // DNN: Not implemented (yet)
                            }
                        }
                        else {
                            label = False;
                        }
                    }
                }   // End of while
            }
        }

        fclose(fn);

        if (debug_level & 16)
            fprintf(stderr,"Closed file\n");
    }
    else {
        fprintf(stderr,"Couldn't open file: %s\n", filename);
    }
}





/**********************************************************
 * draw_map()
 *
 * Function which tries to figure out what type of map or
 * image file we're dealing with, and takes care of getting
 * it onto the screen.  Calls other functions to deal with
 * .geo/.tif/.shp maps.  This function deals with
 * DOS/Windows vector maps directly itself.
 *
 * If destination_pixmap == DRAW_NOT, then we'll not draw
 * the map anywhere, but we'll determine the map extents
 * and write them to the map index file.
 **********************************************************/
void draw_map (Widget w, char *dir, char *filenm, alert_entry * alert,
                unsigned char alert_color, int destination_pixmap,
                int draw_filled) {
    FILE *f;
    char file[MAX_FILENAME];
    char map_it[MAX_FILENAME];

    /* map header info */
    char map_type[5];
    char map_version[5];
    char file_name[33];
    char *ext;
    char map_title[33];
    char map_creator[8];
    unsigned long creation_date;
    unsigned long left_boundary;
    unsigned long right_boundary;
    unsigned long top_boundary;
    unsigned long bottom_boundary;
    char map_reserved1[9];
    long total_vector_points;
    long total_labels;
    char map_reserved2[141];
    char Buffer[2049];
    char *ptr;
    int dos_labels;
    int dos_flag;
    long temp;
    int points_per_degree;
    int map_range;

    /* vector info */
    unsigned char vector_start;
    unsigned char object_behavior;
    unsigned long x_long_cord;
    unsigned long y_lat_cord;

    /* label data */
    char label_type[3];
    unsigned long label_x_cord;
    unsigned long label_y_cord;
    int temp_mag;
    int label_mag;
    char label_symbol_del;
    char label_symbol_char;
    char label_text_color;
    char label_text[50];

    unsigned long year;
    unsigned long days;
    long count;
    int label_length;
    int i;
    int map_maxed_vectors;
    int map_maxed_text_labels;
    int map_maxed_symbol_labels;
    map_vectors *vectors_ptr;
    text_label *text_ptr;
    symbol_label *symbol_ptr;
    int line_width;
    int x, y;
    int color;
    long max_x, max_y;
    int in_window = 0;

    char symbol_table;
    char symbol_id;
    char symbol_color;
    int embedded_object;


    // Skip maps that end in .dbf or .shx
    ext = get_map_ext(filenm);

    if (ext == NULL)
        return;

    // Only crunch data on known map types, skipping all other junk
    // that might be in the map directory.  draw_map() gets called
    // on every file in the map directory during auto_maps runs and
    // during map indexing.
    if (       (strcasecmp(ext,"pdb" ) != 0)
            && (strcasecmp(ext,"map" ) != 0)
            && (strcasecmp(ext,"shp" ) != 0)
            && (strcasecmp(ext,"tif" ) != 0)
            && (strcasecmp(ext,"geo" ) != 0)
            && (strcasecmp(ext,"gnis") != 0) ) {

        // Check whether we're indexing or drawing the map
        if ( (destination_pixmap != INDEX_CHECK_TIMESTAMPS)
                && (destination_pixmap != INDEX_NO_TIMESTAMPS) ) {
            // We're drawing, not indexing.  Output a warning
            // message.
            fprintf(stderr,"*** draw_map: Unknown map type: %s ***\n", filenm);
        }

        return;
    }


    // Check map index
    // Returns: 0 if map is _not_ visible
    //          1 if map _is_ visible
    //          2 if the map is not in the index
    i = map_onscreen_index(filenm);
 
    // Check whether we're indexing or drawing the map
    if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
            || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {

        // We're indexing maps
        if (i != 2) // We already have an index entry for this map.
            // This is where we pick up a big speed increase:
            // Refusing to index a map that's already indexed.
            return; // Skip it.
    }
    else {  // We're drawing maps
        // See if map is visible.  If not, skip it.
        if (i == 0) // Map is not visible, skip it.
            return;
    }


    xastir_snprintf(file, sizeof(file), "%s/%s", dir, filenm);

    // Used for debugging.  If we get a segfault on a map, this is
    // often the only way of finding out which map file we can't
    // handle.
    //fprintf(stderr,"draw_map: %s\n",file);

    x = 0;
    y = 0;
    color = -1;
    line_width = 1;
    mag = (1 * scale_y) / 2;    // determines if details are drawn

    /* MAP counters */
    vectors_ptr = map_vectors_ptr;
    text_ptr = map_text_label_ptr;
    symbol_ptr = map_symbol_label_ptr;

    map_maxed_vectors = 0;
    map_maxed_text_labels = 0;
    map_maxed_symbol_labels = 0;
    npoints = 0;


// Check file extensions for various supported map types:


    // If alert is non-NULL, then we have a weather alert and we need
    // to call draw_shapefile_map() to light up that area.  If alert
    // is NULL, then we decide here what method to use to draw the
    // map.


    // Check for WX alert/ESRI Shapefile maps first
    if ( (alert != NULL)    // We have an alert!
            || ( (ext != NULL) && ( (strcasecmp(ext,"shp") == 0)
                                 || (strcasecmp(ext,"shx") == 0)
                                 || (strcasecmp(ext,"dbf") == 0) ) ) ) { // Or non-alert shapefile map

#ifdef HAVE_LIBSHP
        //fprintf(stderr,"Drawing shapefile map\n");
        if (alert != NULL) {
            //fprintf(stderr,"Alert!\n");
        }
        draw_shapefile_map(w,
            dir,
            filenm,
            alert,
            alert_color,
            destination_pixmap,
            draw_filled);
#else   // HAVE_LIBSHP
        if (!alert) {   // Skip message if alert (too many!)
            fprintf(stderr,"*** Shapelib support is not compiled in! ***\n");
        }
        return;
#endif  // HAVE_LIBSHP
    }


    // .geo image map? (can be one of many formats)
    else if (ext != NULL && strcasecmp (ext, "geo") == 0) {
#ifndef NO_GRAPHICS
        draw_geo_image_map(w,
            dir,
            filenm,
            destination_pixmap);
#else   // NO_GRAPHICS
        fprintf(stderr,"*** No XPM or ImageMagick support compiled into Xastir! ***\n");
        return;
#endif  // NO_GRAPHICS
    }


    // USGS DRG geoTIFF map?
    else if (ext != NULL && strcasecmp (ext, "tif") == 0) {
#ifdef HAVE_LIBGEOTIFF
        draw_geotiff_image_map(w,
            dir,
            filenm,
            destination_pixmap);
#else   // HAVE_LIBGEOTIFF
        fprintf(stderr,"*** Libgeotiff support is not compiled in!  ***\n");
        return;
#endif // HAVE_LIBGEOTIFF
    }


    // Palm map?
    else if (ext != NULL && strcasecmp (ext, "pdb") == 0) {
        //fprintf(stderr,"calling draw_palmimage_map: %s/%s\n", dir, filenm);
        draw_palm_image_map(w,
            dir,
            filenm,
            destination_pixmap,
            draw_filled);
    }


    // GNIS database file?
    else if (ext != NULL && strcasecmp (ext, "gnis") == 0) {
        draw_gnis_map(w,
            dir,
            filenm,
            destination_pixmap);
    }


    // Else must be APRSdos or WinAPRS map
    else if (ext != NULL && strcasecmp (ext, "MAP") == 0) {
        f = fopen (file, "r");
        if (f != NULL) {
            (void)fread (map_type, 4, 1, f);
            map_type[4] = '\0';
            dos_labels = FALSE;
            points_per_degree = 300;

            // DOS-type map header portion of the code.
            if (strtod (map_type, &ptr) > 0.01 && (*ptr == '\0' || *ptr == ' ' || *ptr == ',')) {
                int j;

                if (debug_level & 512)
                    fprintf(stderr,"DOS Map\n");

//fprintf(stderr,"DOS Map\n");

                top_boundary = left_boundary = bottom_boundary = right_boundary = 0;
                rewind (f);
                map_title[0] = map_creator[0] = Buffer[0] = '\0';
                strncpy (map_type, "DOS ", 4);          // set map_type for DOS ASCII maps
                strncpy (file_name, filenm, 32);
                total_vector_points = 200000;
                total_labels = 2000;

// Lclint can't handle this structure for some reason.
#ifndef __LCLINT__
                for (j = 0; j < DOS_HDR_LINES; strlen (Buffer) ? 1: j++) {
#else   // __LCLINT__
// So we do it this way for Lclint:
                for (j = 0; j < DOS_HDR_LINES;) {
                    if (!strlen(Buffer))
                        j++;
#endif // __LCLINT__

                    (void)fgets (&Buffer[strlen (Buffer)],(int)sizeof (Buffer) - (strlen (Buffer)), f);
                    while ((ptr = strpbrk (Buffer, "\r\n")) != NULL && j < DOS_HDR_LINES) {
                        *ptr = '\0';
                        for (ptr++; *ptr == '\r' || *ptr == '\n'; ptr++) ;
                        switch (j) {
                            case 0:
                                top_boundary = (unsigned long) (-atof (Buffer) * 360000 + 32400000);
                                break;

                            case 1:
                                left_boundary = (unsigned long) (-atof (Buffer) * 360000 + 64800000);
                                break;

                            case 2:
                                points_per_degree = (int) atof (Buffer);
                                break;

                            case 3:
                                bottom_boundary = (unsigned long) (-atof (Buffer) * 360000 + 32400000);
                                bottom_boundary = bottom_boundary + bottom_boundary - top_boundary;
                                break;

                            case 4:
                                right_boundary = (unsigned long) (-atof (Buffer) * 360000 + 64800000);
                                right_boundary = right_boundary + right_boundary - left_boundary;
                                break;

                            case 5:
                                map_range = (int) atof (Buffer);
                                break;

                            case 7:
                                strncpy (map_version, Buffer, 4);
                                break;
                        }
                        strcpy (Buffer, ptr);
                        if (strlen (Buffer))
                            j++;
                    }
                }   // End of DOS-type map header portion

            } else {
                // Windows-type map header portion

                if (debug_level & 512)
                    fprintf(stderr,"Windows map\n");

//fprintf(stderr,"Windows map\n");

                (void)fread (map_version, 4, 1, f);
                map_version[4] = '\0';

                (void)fread (file_name, 32, 1, f);
                file_name[32] = '\0';

                (void)fread (map_title, 32, 1, f);
                map_title[32] = '\0';
                if (debug_level & 16)
                    fprintf(stderr,"Map Title %s\n", map_title);

                (void)fread (map_creator, 8, 1, f);
                map_creator[8] = '\0';
                if (debug_level & 16)
                    fprintf(stderr,"Map Creator %s\n", map_creator);

                (void)fread (&temp, 4, 1, f);
                creation_date = ntohl (temp);
                if (debug_level & 16)
                    fprintf(stderr,"Creation Date %lX\n", creation_date);

                year = creation_date / 31536000l;
                days = (creation_date - (year * 31536000l)) / 86400l;
                if (debug_level & 16)
                    fprintf(stderr,"year is %ld + days %ld\n", 1904l + year, (long)days);

                (void)fread (&temp, 4, 1, f);
                left_boundary = ntohl (temp);

                (void)fread (&temp, 4, 1, f);
                right_boundary = ntohl (temp);

                (void)fread (&temp, 4, 1, f);
                top_boundary = ntohl (temp);

                (void)fread (&temp, 4, 1, f);
                bottom_boundary = ntohl (temp);

                if (strcmp (map_version, "2.00") != 0) {
                    left_boundary *= 10;
                    right_boundary *= 10;
                    top_boundary *= 10;
                    bottom_boundary *= 10;
                }
                (void)fread (map_reserved1, 8, 1, f);
                (void)fread (&temp, 4, 1, f);
                total_vector_points = (long)ntohl (temp);
                (void)fread (&temp, 4, 1, f);
                total_labels = (long)ntohl (temp);
                (void)fread (map_reserved2, 140, 1, f);

            }   // End of Windows-type map header portion


            // Done processing map header info.  The rest of this
            // function performs the actual drawing of both DOS-type
            // and Windows-type maps to the screen.


            if (debug_level & 16) {
                fprintf(stderr,"Map Type %s, Version: %s, Filename %s\n", map_type,map_version, file_name);
                fprintf(stderr,"Left Boundary %ld, Right Boundary %ld\n", (long)left_boundary,(long)right_boundary);
                fprintf(stderr,"Top Boundary %ld, Bottom Boundary %ld\n", (long)top_boundary,(long)bottom_boundary);
                fprintf(stderr,"Total vector points %ld, total labels %ld\n",total_vector_points, total_labels);
            }


            // Check whether we're indexing or drawing the map
            if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
                    || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {

                // We're indexing only.  Save the extents in the index.
                index_update_xastir(filenm, // Filename only
                    bottom_boundary,        // Bottom
                    top_boundary,           // Top
                    left_boundary,          // Left
                    right_boundary);        // Right

                (void)fclose (f);
 
                return; // Done indexing this file
            }


            in_window = map_onscreen(left_boundary, right_boundary, top_boundary, bottom_boundary);

            if (in_window) {
                unsigned char last_behavior, special_fill = (unsigned char)FALSE;
                object_behavior = '\0';


                // Check whether we're indexing or drawing the map
                if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
                        || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {
                    xastir_snprintf(map_it, sizeof(map_it), langcode ("BBARSTA039"), filenm);
                }
                else {
                    xastir_snprintf(map_it, sizeof(map_it), langcode ("BBARSTA028"), filenm);
                }
                statusline(map_it,0);       // Loading/Indexing ...


                if (debug_level & 16)
                    fprintf(stderr,"in Boundary %s\n", map_it);

                (void)XSetLineAttributes (XtDisplay (w), gc, line_width, LineSolid, CapButt,JoinMiter);

                /* read vectors */
                max_x = screen_width  + MAX_OUTBOUND;
                max_y = screen_height + MAX_OUTBOUND;

                x_long_cord = 0;
                y_lat_cord  = 0;
                color = 0;
                dos_flag = 0;
                for (count = 0l;count < total_vector_points && !feof (f) && !dos_labels; count++) {
                    if (strncmp ("DOS ", map_type, 4) == 0) {
                        (void)fgets (&Buffer[strlen (Buffer)],(int)sizeof (Buffer) - (strlen (Buffer)), f);
                        while ((ptr = strpbrk (Buffer, "\r\n")) != NULL && !dos_labels) {
                            long LatHld = 0, LongHld;
                            char *trailer;
                            *ptr = '\0';
                            for (ptr++; *ptr == '\r' || *ptr == '\n'; ptr++) ;

                            process:

                            if (strncasecmp ("Line", map_version, 4) == 0) {
                                int k;
                                color = (int)strtol (Buffer, &trailer, 0);
                                if (trailer && (*trailer == ',' || *trailer == ' ')) {
                                    trailer++;
                                    if (color == -1) {
                                        dos_labels = (int)TRUE;
                                        strcpy (Buffer, ptr);
                                        break;
                                    }
                                    for (k = strlen (trailer) - 1; k >= 0; k--)
                                        trailer[k] = (char)( (int)trailer[k] - 27 );

                                    while (*trailer) {
                                        LongHld = (long)( (int)(*(unsigned char *)trailer) * 16);
                                        trailer++;
                                        LatHld = (long)( (int)(*(unsigned char *)trailer) * 8);
                                        trailer++;
                                        LongHld += (long)((*trailer >> 3) & 0xf);
                                        LatHld += (long)( (*trailer) & 0x7);
                                        trailer++;
                                        LatHld = ((double)LatHld * 360000.0) / points_per_degree;
                                        LongHld = ((double)LongHld * 360000.0) / points_per_degree;
                                        x_long_cord = LongHld + left_boundary;
                                        y_lat_cord = LatHld + top_boundary;
                                        map_plot (w,
                                            max_x,
                                            max_y,
                                            (long)x_long_cord,
                                            (long)y_lat_cord,
                                            (unsigned char)color,
                                            0,
                                            destination_pixmap,
                                            draw_filled);
                                    }
                                    map_plot (w,
                                        max_x,
                                        max_y,
                                        (long)x_long_cord,
                                        (long)y_lat_cord,
                                        '\0',
                                        0,
                                        destination_pixmap,
                                        draw_filled);
                                }
                            } else if (strncasecmp ("ASCII", map_version, 4) == 0) {
                                if (color == 0) {
                                    color = (int)strtol (Buffer, &trailer, 0);
                                    if (trailer && strpbrk (trailer, ", ")) {
                                        for (; *trailer == ',' || *trailer == ' '; trailer++) ;
                                        dos_flag = (int)strtol (trailer, &trailer, 0);
                                        if (dos_flag == -1)
                                            dos_labels = (int)TRUE;
                                    }
                                } else {
                                    LongHld = strtol (Buffer, &trailer, 0);
                                    if (trailer && strpbrk (trailer, ", ")) {
                                        for (; *trailer == ',' || *trailer == ' '; trailer++) ;
                                        LatHld = strtol (trailer, &trailer, 0);
                                    } else if (LongHld == 0 && *trailer != '\0') {
                                        strncpy (map_version, "Comp", 4);
                                        goto process;
                                    }
                                    if (LongHld == 0 && LatHld == 0) {
                                        color = 0;
                                        map_plot (w,
                                            max_x,
                                            max_y,
                                            (long)x_long_cord,
                                            (long)y_lat_cord,
                                            (unsigned char)color,
                                            0,
                                            destination_pixmap,
                                            draw_filled);
                                    } else if (LongHld == 0 && LatHld == -1) {
                                        dos_labels = (int)TRUE;
                                        map_plot (w,
                                            max_x,
                                            max_y,
                                            (long)x_long_cord,
                                            (long)y_lat_cord,
                                            '\0',
                                            0,
                                            destination_pixmap,
                                            draw_filled);
                                    } else {
                                        LatHld = ((double)LatHld * 360000.0) / points_per_degree;
                                        LongHld = ((double)LongHld * 360000.0) / points_per_degree;
                                        x_long_cord = LongHld + left_boundary;
                                        y_lat_cord = LatHld + top_boundary;
                                        map_plot (w,
                                            max_x,
                                            max_y,
                                            (long)x_long_cord,
                                            (long)y_lat_cord,
                                            (unsigned char)color,
                                            0,
                                            destination_pixmap,
                                            draw_filled);
                                    }
                                }
                            } else if (strncasecmp ("Comp", map_version, 4) == 0) {
                                char Tag[81];
                                int k;
                                Tag[80] = '\0';
                                if (color == 0) {
                                    color = (int)strtol (Buffer, &trailer, 0);
                                    if (trailer && strpbrk (trailer, ", ")) {
                                        for (; *trailer == ',' || *trailer == ' '; trailer++) ;
                                        dos_flag = (int)strtol (trailer, &trailer, 0);
                                        strncpy (Tag, trailer, 80);
                                        if (dos_flag == -1)
                                            dos_labels = (int)TRUE;
                                    }
                                } else {
                                    LongHld = strtol (Buffer, &trailer, 0);
                                    for (; *trailer == ',' || *trailer == ' '; trailer++) ;
                                    LatHld = strtol (trailer, &trailer, 0);
                                    if (LatHld == 0 && *trailer != '\0')
                                        LatHld = 1;

                                    if (LongHld == 0 && LatHld == 0) {
                                        color = 0;
                                        map_plot (w,
                                            max_x,
                                            max_y,
                                            (long)x_long_cord,
                                            (long)y_lat_cord,
                                            (unsigned char)color,
                                            0,
                                            destination_pixmap,
                                            draw_filled);
                                    } else if (LongHld == 0 && LatHld == -1) {
                                        dos_labels = (int)TRUE;
                                        map_plot (w,
                                            max_x,
                                            max_y,
                                            (long)x_long_cord,
                                            (long)y_lat_cord,
                                            (unsigned char)color,
                                            0,
                                            destination_pixmap,
                                            draw_filled);
                                    }

                                    if (color && !dos_labels) {
                                        trailer = Buffer;
                                        for (k = strlen (trailer) - 1; k >= 0; k--)
                                            trailer[k] = (char)((int)trailer[k] - 27);

                                        while (*trailer) {
                                            LongHld = (long)( (int)(*(unsigned char *)trailer) * 16);
                                            trailer++;
                                            LatHld = (long)( (int)(*(unsigned char *)trailer) * 8);
                                            trailer++;
                                            LongHld += (long)((*(unsigned char *)trailer >> 3) & 0xf);
                                            LatHld += (*trailer) & 7l;
                                            trailer++;
                                            LatHld = ((double)LatHld * 360000.0) / points_per_degree;
                                            LongHld = ((double)LongHld * 360000.0) / points_per_degree;
                                            x_long_cord = LongHld + left_boundary;
                                            y_lat_cord = LatHld + top_boundary;
                                            map_plot (w,
                                                max_x,
                                                max_y,
                                                (long)x_long_cord,
                                                (long)y_lat_cord,
                                                (unsigned char)color,
                                                0,
                                                destination_pixmap,
                                                draw_filled);
                                        }
                                    }
                                }
                            } else {
                                LongHld = strtol (Buffer, &trailer, 0);
                                if (trailer) {
                                    if (*trailer == ',' || *trailer == ' ') {
                                        if (LongHld == 0)
                                            strncpy (map_version, "ASCII", 4);

                                        trailer++;
                                        dos_flag = (int)strtol (trailer, &trailer, 0);
                                        if (dos_flag == -1)
                                            dos_labels = (int)TRUE;

                                        if (dos_flag == 0 && *trailer != '\0') {
                                            strncpy (map_version, "Line", 4);
                                            goto process;
                                        }
                                        color = (int)LongHld;
                                    }
                                } else
                                    strncpy (map_version, "Comp", 4);
                            }
                            strcpy (Buffer, ptr);
                        }
                    } else {    // Windows map...
                        last_behavior = object_behavior;
                        (void)fread (&vector_start, 1, 1, f);
                        (void)fread (&object_behavior, 1, 1, f);      // Fill Color?
                        if (strcmp (map_type, "COMP") == 0) {
                            short temp_short;
                            long LatOffset, LongOffset;
                            LatOffset  = (long)(top_boundary  - top_boundary  % 6000);
                            LongOffset = (long)(left_boundary - left_boundary % 6000);
                            (void)fread (&temp_short, 2, 1, f);
                            x_long_cord = (ntohs (temp_short) * 10 + LongOffset);
                            (void)fread (&temp_short, 2, 1, f);
                            y_lat_cord  = (ntohs (temp_short) * 10 + LatOffset);
                        } else {
                            (void)fread (&temp, 4, 1, f);
                            x_long_cord = ntohl (temp);
                            if (strcmp (map_version, "2.00") != 0)
                                x_long_cord *= 10;

                            (void)fread (&temp, 4, 1, f);
                            y_lat_cord = ntohl (temp);
                            if (strcmp (map_version, "2.00") != 0)
                                y_lat_cord *= 10;
                        }
                        if (alert_color && last_behavior & 0x80 && (int)vector_start == 0xff) {
                            map_plot (w,
                                max_x,
                                max_y,
                                (long)x_long_cord,
                                (long)y_lat_cord,
                                '\0',
                                (long)alert_color,
                                destination_pixmap,
                                draw_filled);
                            //special_fill = TRUE;
                        }
                        map_plot (w,
                            max_x,
                            max_y,
                            (long)x_long_cord,
                            (long)y_lat_cord,
                            vector_start,
                            (long)object_behavior,
                            destination_pixmap,
                            draw_filled);
                    }
                }
                if (alert_color)
                    map_plot (w,
                        max_x,
                        max_y,
                        0,
                        0,
                        '\0',
                        special_fill ? (long)0xfd : (long)alert_color,
                        destination_pixmap,
                        draw_filled);
                else
                    map_plot (w,
                    max_x,
                    max_y,
                    0,
                    0,
                    (unsigned char)0xff,
                    0,
                    destination_pixmap,
                    draw_filled);

                (void)XSetForeground (XtDisplay (w), gc, colors[20]);
                line_width = 2;
                (void)XSetLineAttributes (XtDisplay (w), gc, line_width, LineSolid, CapButt,JoinMiter);


                // Here is the map label section of the code for both DOS & Windows-type maps
                if (map_labels) {
                    /* read labels */
                    for (count = 0l; count < total_labels && !feof (f); count++) {
                        //DOS-Type Map Labels
                        embedded_object = 0;
                        if (strcmp (map_type, "DOS ") == 0) {   // Handle DOS-type map labels/embedded objects
                            char *trailer;
                            (void)fgets (&Buffer[strlen (Buffer)],(int)sizeof (Buffer) - (strlen (Buffer)), f);
                            for (; (ptr = strpbrk (Buffer, "\r\n")) != NULL;strcpy (Buffer, ptr)) {
                                *ptr = '\0';
                                label_type[0] = (char)0x08;
                                for (ptr++; *ptr == '\r' || *ptr == '\n'; ptr++) ;
                                trailer = strchr (Buffer, ',');
                                if (trailer && strncmp (Buffer, "0", 1) != 0) {
                                    *trailer = '\0';
                                    trailer++;
                                    strcpy (label_text, Buffer);
 
                                    // Check for '#' or '$' as the first character of the label.
                                    // If found, we have an embedded symbol and colored text to display.
                                    symbol_table = ' ';
                                    symbol_id = ' ';
                                    symbol_color = '0';
                                    if ( (label_text[0] == '$') || (label_text[0] == '#') ) {
                                        // We found an embedded map object
                                        embedded_object = 1;                        // Set the flag
                                        if (label_text[0] == '$') {                  // Old format: $xC
                                            symbol_table = '/';
                                            symbol_id = label_text[1];
                                            symbol_color = label_text[2];
                                            strcpy( label_text, Buffer+3 );         // Take the object out of the label text
                                        }
                                        else {  // Could be in new or old format with a leading '#' character
                                            symbol_table = label_text[1];
                                            if (symbol_table == '/' || symbol_table == '\\') {  // New format: #/xC
                                                symbol_id = label_text[2];
                                                symbol_color = label_text[3];
                                                strcpy( label_text, Buffer+4 );     // Take the object out of the label text
                                            }
                                            else {                                  // Old format: #xC
                                                symbol_table = '\\';
                                                symbol_id = label_text[1];
                                                symbol_color = label_text[2];
                                                strcpy( label_text, Buffer+3 );     // Take the object out of the label text
                                            }
                                        }
                                        if (debug_level & 512)
                                            fprintf(stderr,"Found embedded object: %c %c %c %s\n",symbol_table,symbol_id,symbol_color,label_text);
                                    }


                                    label_length = (int)strlen (label_text);
                                    label_y_cord = (unsigned long) (-strtod (trailer, &trailer) * 360000) + 32400000;
                                    trailer++;
                                    label_x_cord = (unsigned long) (-strtod (trailer, &trailer) * 360000) + 64800000;
                                    trailer++;
                                    label_mag = (int)strtol (trailer, &trailer, 0) * 20;
                                    if ((label_type[0] & 0x80) == '\0') /* left of coords */
                                        x = ((label_x_cord - x_long_offset) / scale_x) - (label_length * 6);
                                    else  /* right of coords */
                                        x = ((label_x_cord - x_long_offset) / scale_x);

                                    y = ((label_y_cord - y_lat_offset) / scale_y);
                                    if (x > (0) && (x < (int)screen_width)) {
                                        if (y > (0) && (y < (int)screen_height)) {
                                            /*fprintf(stderr,"Label mag %d mag %d\n",label_mag,(scale_x*2)-1); */
                                            //if (label_mag > (int)((scale_x * 2) - 1) || label_mag == 0)
                                            if (label_mag > (int)((scale_x) - 1) || label_mag == 0) {
                                                if (embedded_object) {
                                                    // NOTE: 0x21 is the first color for the area object or "DOS" colors
                                                    draw_label_text (w, x+10, y+5, label_length,colors[0x21 + symbol_color],label_text);
                                                    symbol(w,0,symbol_table,symbol_id,' ',pixmap,1,x-10,y-10,' ');
                                                }
                                                else {
                                                    draw_label_text (w, x, y, label_length,colors[(int)(label_type[0] & 0x7f)],label_text);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        } else {    // Handle Windows-type map labels/embedded objects
                            int rotation = 0;
                            char rotation_factor[5];

                            // Windows-Type Map Labels
                            char label_type_1[2], label_type_2[2];

                            // Snag first two bytes of label
                            (void)fread (label_type_1, 1, 1, f);
                            (void)fread (label_type_2, 1, 1, f);

                            if (label_type_2[0] == '\0') {  // Found a label

                                // Found text label
                                (void)fread (&temp, 4, 1, f);           /* x */
                                label_x_cord = ntohl (temp);
                                if (strcmp (map_version, "2.00") != 0)
                                    label_x_cord *= 10;

                                (void)fread (&temp, 4, 1, f);           /* y */
                                label_y_cord = ntohl (temp);
                                if (strcmp (map_version, "2.00") != 0)
                                    label_y_cord *= 10;

                                (void)fread (&temp_mag, 2, 1, f);       /* mag */
                                label_mag = (int)ntohs (temp_mag);
                                if (strcmp (map_version, "2.00") != 0)
                                    label_mag *= 10;

                                if (strcmp (map_type, "COMP") == 0) {
                                    for (i = 0; i < 32; i++) {
                                        (void)fread (&label_text[i], 1, 1, f);
                                        if (label_text[i] == '\0') {
                                            break;
                                        }
                                    }
                                    label_text[32] = '\0';  // Make sure we have a terminator
                                }
                                else {
                                    (void)fread (label_text, 32, 1, f);   /* text */
                                    label_text[32] = '\0';  // Make sure we have a terminator
                                }


                                // Special strings like: "#123" are rotation factors for labels
                                // in degrees.  This is not documented in the windows-type map
                                // format documents that I could find.
                                if (label_text[0] == '#') {
                                    int i,j;
                                    if (debug_level & 512)
                                        fprintf(stderr,"%s\n",label_text);

                                    // Save the rotation factor in "rotation"
                                    for ( i=1; i<4; i++ )
                                        rotation_factor[i-1] = label_text[i];
                                    rotation_factor[3] = '\0';
                                    rotation = atoi(rotation_factor);

                                    // Take rotation factor out of label string
                                    for ( i=4, j=0; i < (int)(strlen(label_text)+1); i++,j++)
                                        label_text[j] = label_text[i];

                                    //fprintf(stderr,"Windows label: %s, rotation factor: %d\n",label_text, rotation);
                                }


                                label_length = (int)strlen (label_text);

                                for (i = (label_length - 1); i > 0; i--) {
                                    if (label_text[i] == ' ')
                                        label_text[i] = '\0';
                                    else
                                        break;
                                }

                                label_length = (int)strlen (label_text);
                                /*fprintf(stderr,"labelin:%s\n",label_text); */

                                if ((label_type_1[0] & 0x80) == '\0') {
                                    /* left of coords */
                                    x = ((label_x_cord - x_long_offset) / scale_x) - (label_length * 6);
                                    x = 0;  // ??????
                                } else {
                                    /* right of coords */
                                    x = ((label_x_cord - x_long_offset) / scale_x);
                                }

                                y = ((label_y_cord - y_lat_offset) / scale_y);

                                if (x > (0) && (x < (int)screen_width)) {
                                    if (y > (0) && (y < (int)screen_height)) {
                                        /*fprintf(stderr,"Label mag %d mag %d\n",label_mag,(scale_x*2)-1); */
                                        //if (label_mag > (int)((scale_x * 2) - 1) || label_mag == 0)
                                        if (label_mag > (int)((scale_x) - 1) || label_mag == 0) {
// Note: We're not drawing the labels in the right colors
                                            if (rotation == 0) {    // Non-rotated label
//                                                draw_label_text (w,
//                                                    x,
//                                                    y,
//                                                    label_length,
//                                                    colors[(int)(label_type_1[0] & 0x7f)],
//                                                    label_text);
                                                draw_rotated_label_text (w,
                                                    -90.0,
                                                    x,
                                                    y,
                                                    label_length,
                                                    colors[(int)(label_type_1[0] & 0x7f)],
                                                    label_text);
                                            }
                                            else {  // Rotated label
                                                draw_rotated_label_text (w,
                                                    rotation,
                                                    x,
                                                    y,
                                                    label_length,
                                                    colors[(int)(label_type_1[0] & 0x7f)],
                                                    label_text);
                                            }
                                        }
                                    }
                                }
                            } else if (label_type_2[0] == '\1' && label_type_1[0] == '\0'){ // Found an embedded object

//fprintf(stderr,"Found windows embedded symbol\n");

                                /* label is an embedded symbol */
                                (void)fread (&temp, 4, 1, f);
                                label_x_cord = ntohl (temp);
                                if (strcmp (map_version, "2.00") != 0)
                                    label_x_cord *= 10;

                                (void)fread (&temp, 4, 1, f);
                                label_y_cord = ntohl (temp);
                                if (strcmp (map_version, "2.00") != 0)
                                    label_y_cord *= 10;

                                (void)fread (&temp_mag, 2, 1, f);
                                label_mag = (int)ntohs (temp_mag);
                                if (strcmp (map_version, "2.00") != 0)
                                    label_mag *= 10;

                                (void)fread (&label_symbol_del, 1, 1, f);   // Snag symbol table char
                                (void)fread (&label_symbol_char, 1, 1, f);  // Snag symbol char
                                (void)fread (&label_text_color, 1, 1, f);   // Snag text color (should be 1-9, others should default to black)
                                if (label_text_color < '1' && label_text_color > '9')
                                    label_text_color = '0'; // Default to black

                                x = ((label_x_cord - x_long_offset) / scale_x);
                                y = ((label_y_cord - y_lat_offset) / scale_y);

                                // Read the label text portion
                                if (strcmp (map_type, "COMP") == 0) {
                                    for (i = 0; i < 32; i++) {
                                        (void)fread (&label_text[i], 1, 1, f);
                                        if (label_text[i] == '\0')
                                            break;
                                    }
                                }
                                else {
                                    (void)fread (label_text, 29, 1, f);
                                }

                                // NOTE: 0x21 is the first color for the area object or "DOS" colors
                                draw_label_text (w, x+10, y+5, strlen(label_text),colors[0x21 + label_text_color],label_text);
                                symbol(w,0,label_symbol_del,label_symbol_char,' ',pixmap,1,x-10,y-10,' ');

                                if (debug_level & 512)
                                    fprintf(stderr,"Windows map, embedded object: %c %c %c %s\n",
                                        label_symbol_del,label_symbol_char,label_text_color,label_text);
                            }
                            else {
                                if (debug_level & 512)
                                    fprintf(stderr,"Weird label in Windows map, neither a plain label nor an object: %d %d\n",
                                        label_type_1[0],label_type_2[0]);
                            }
                        }
                    }
                }       // if (map_labels)
            }
            (void)fclose (f);
        }
        else
            fprintf(stderr,"Couldn't open file: %s\n", file);
    }


    // Look for well-known non-map types:
    else if (ext == NULL) {
        // Do nothing, no file extension to work from.
    }
 

    // Look for well-known non-map types.  Note that this code
    // doesn't get reached for these types anymore due to the code
    // at the top that exits the function if the file extension is
    // not one of the known map types.
    else if ( (ext != NULL)
            && (   (strlen(ext) == 0)
                || (strcasecmp (ext, "html" ) == 0)  // html
                || (strcasecmp (ext, "htm"  ) == 0)   // html
                || (strcasecmp (ext, "zip"  ) == 0)   // Could be a map type someday?
                || (strcasecmp (ext, "fgd"  ) == 0)   // geotiff
                || (strcasecmp (ext, "tfw"  ) == 0)   // geotiff
                || (strcasecmp (ext, "gif"  ) == 0)   // .geo files
                || (strcasecmp (ext, "jpg"  ) == 0)   // .geo files
                || (strcasecmp (ext, "miff" ) == 0)  // .geo files
                || (strcasecmp (ext, "xpm"  ) == 0)   // .geo files
                || (strcasecmp (ext, "dbf"  ) == 0)   // Shapefiles
                || (strcasecmp (ext, "txt"  ) == 0)   // text
                || (strcasecmp (ext, "prc"  ) == 0)   // Palm executable
                || (strcasecmp (ext, "exe"  ) == 0)   // DOS/Win executable
                || (strcasecmp (ext, "inf"  ) == 0)
                || (strcasecmp (ext, "Z"    ) == 0)     // Could be a map type someday?
                || (strcasecmp (ext, "gz"   ) == 0)    // Could be a map type someday?
                || (strcasecmp (ext, "pl"   ) == 0)    // Perl
                || (strcasecmp (ext, "prj"  ) == 0)   // Project file
                || (strcasecmp (ext, "tgz"  ) == 0)   // Could be a map type someday?
                || (strcasecmp (ext, "pos"  ) == 0)
                || (strcasecmp (ext, "hst"  ) == 0)
                || (strcasecmp (ext, "bk"   ) == 0)    // Backup file
                || (strcasecmp (ext, "bak"  ) == 0)   // Backup file
                || (strcasecmp (ext, "dx"   ) == 0)
                || (strcasecmp (ext, "geotiff") == 0)
                || (strcasecmp (ext, "xml"  ) == 0)   // XML file
                || (strcasecmp (ext, "bmp"  ) == 0)   // .geo files
                || (strcasecmp (ext, "dat"  ) == 0)
                || (strcasecmp (ext, "placeholder") == 0)
                || (strcasecmp (ext, "hi"   ) == 0)
                || (strcasecmp (ext, "jgw"  ) == 0)   // we7u
                || (strcasecmp (ext, "bad"  ) == 0)   // we7u
                || (strcasecmp (ext, "new"  ) == 0)   // we7u
                || (strcasecmp (ext, "bmp24") == 0) // we7u
                || (strcasecmp (ext, "works") == 0) // we7u
                || (strcasecmp (ext, "gif87") == 0) // we7u
                || (strcasecmp (ext, "sbn"  ) == 0)
                || (strcasecmp (ext, "sbx"  ) == 0)
                || (strcasecmp (ext, "cmd"  ) == 0) ) ) { // Windows script

        // Do nothing with these types of files
    }


    // Couldn't figure out the map type.  Check whether we're
    // indexing or drawing the map.
    else if ( (destination_pixmap != INDEX_CHECK_TIMESTAMPS)
            && (destination_pixmap != INDEX_NO_TIMESTAMPS) ) {
        // We're drawing, not indexing.  Output a warning
        // message.
        fprintf(stderr,"*** draw_map: Unknown map type: %s ***\n", filenm);
    }

    XmUpdateDisplay (XtParent (da));
}  // End of draw_map()





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
void map_search (Widget w, char *dir, alert_entry * alert, int *alert_count,int warn, int destination_pixmap) {
    struct dirent *dl = NULL;
    DIR *dm;
    char fullpath[MAX_FILENAME];
    struct stat nfile;
    const time_t *ftime;
    char this_time[40];
    char *ptr;
    char *map_dir;
    int map_dir_length;

    // We'll use the weather alert directory if it's an alert
    map_dir = alert ? ALERT_MAP_DIR : SELECTED_MAP_DIR;

    map_dir_length = (int)strlen (map_dir);

    if (alert) {    // We're doing weather alerts
        // First check whether the alert->filename variable is filled
        // in.  If so, we've already found the file and can just display
        // that shape in the file
        if (alert->filename[0] == '\0') {   // No filename in struct, so will have
                                            // to search for the shape in the files.
            switch (alert->title[3]) {
                case 'C':   // 'C' in 4th char means county
                    // Use County file c_??????
                    //fprintf(stderr,"%c:County file\n",alert->title[3]);
                    strncpy (alert->filename, "c_", sizeof (alert->filename));
                    break;
                case 'A':   // 'A' in 4th char means county warning area
                    // Use County warning area w_?????
                    //fprintf(stderr,"%c:County warning area file\n",alert->title[3]);
                    strncpy (alert->filename, "w_", sizeof (alert->filename));
                    break;
                case 'Z':
                    // Zone, coastal or offshore marine zone file z_????? or mz?????? or oz??????
                    // oz: ANZ081-086,088,PZZ081-085
                    // mz: AM,AN,GM,LC,LE,LH,LM,LO,LS,PH,PK,PM,PS,PZ,SL
                    // z_: All others
                    if (strncasecmp(alert->title,"AM",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"AN",2) == 0) {
                        // Need to check for Z081-Z086, Z088, if so use
                        // oz??????, else use mz??????
                        if (       (strncasecmp(&alert->title[3],"Z081",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z082",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z083",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z084",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z085",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z086",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z088",4) == 0) ) {
                            //fprintf(stderr,"%c:Offshore marine zone file\n",alert->title[3]);
                            strncpy (alert->filename, "oz", sizeof (alert->filename));
                        }
                        else {
                            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                            strncpy (alert->filename, "mz", sizeof (alert->filename));
                        }
                    }
                    else if (strncasecmp(alert->title,"GM",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LC",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LE",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LH",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LM",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LO",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LS",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"PH",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"PK",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"PM",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"PS",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"PZ",2) == 0) {
// Need to check for PZZ081-085, if so use oz??????, else use mz??????
                        if (       (strncasecmp(&alert->title[3],"Z081",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z082",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z083",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z084",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z085",4) == 0) ) {
                            //fprintf(stderr,"%c:Offshore marine zone file\n",alert->title[3]);
                            strncpy (alert->filename, "oz", sizeof (alert->filename));
                        }
                        else {
                            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                            strncpy (alert->filename, "mz", sizeof (alert->filename));
                        }
                    }
                    else if (strncasecmp(alert->title,"SL",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else {
                        // Must be regular zone file instead of coastal
                        // marine zone or offshore marine zone.
                        //fprintf(stderr,"%c:Zone file\n",alert->title[3]);
                        strncpy (alert->filename, "z_", sizeof (alert->filename));
                    }
                    break;
                default:
                    // Unknown type
//fprintf(stderr,"%c:Can't match weather warning to a Shapefile:%s\n",alert->title[3],alert->title);
                    break;
            }
//            fprintf(stderr,"%s\t%s\t%s\n",alert->activity,alert->alert_status,alert->title);
            //fprintf(stderr,"File: %s\n",alert->filename);
        }

// NOTE:  Need to skip this part if we have a full filename.

        if (alert->filename[0]) {   // We have at least a partial filename
            int done = 0;

            if (strlen(alert->filename) > 3)
                done++; // We already have a filename

            if (!done) {    // We don't have a filename yet

                // Look through the warning directory to find a match for
                // the first few characters that we already figured out.
                // This is designed so that we don't need to know the exact
                // filename, but only the lead three characters in order to
                // figure out which shapefile to use.
                dm = opendir (dir);
                if (!dm) {  // Couldn't open directory
                    xastir_snprintf(fullpath, sizeof(fullpath), "aprsmap %s", dir);
                    // If local alert, warn the operator via the
                    // console as well.
                    if (warn)
                        perror (fullpath);
                }
                else {    // We could open the directory just fine
                    while ( (dl = readdir(dm)) && !done ) {
                        int i;

                        // Check the file/directory name for control
                        // characters
                        for (i = 0; i < (int)strlen(dl->d_name); i++) {
                            // Dump out a warning if control
                            // characters other than LF or CR are
                            // found.
                            if ( (dl->d_name[i] != '\n')
                                    && (dl->d_name[i] != '\r')
                                    && (dl->d_name[i] < 0x20) ) {

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
                        if (stat (fullpath, &nfile) == 0) {
                            ftime = (time_t *)&nfile.st_ctime;
                            switch (nfile.st_mode & S_IFMT) {
                                case (S_IFDIR):     // It's a directory, skip it
                                    break;

                                case (S_IFREG):     // It's a file, check it
                                    /*fprintf(stderr,"FILE %s\n",dl->d_name); */
                                    // Here we look for a match for the
                                    // first 2 characters of the filename.
                                    // 
                                    if (strncasecmp(alert->filename,dl->d_name,2) == 0) {
                                        // We have a match
                                        //fprintf(stderr,"%s\n",fullpath);
                                        // Force last three characters to
                                        // "shp"
                                        dl->d_name[strlen(dl->d_name)-3] = 's';
                                        dl->d_name[strlen(dl->d_name)-2] = 'h';
                                        dl->d_name[strlen(dl->d_name)-1] = 'p';

                                        // Save the filename in the alert
                                        strncpy(alert->filename,dl->d_name,strlen(dl->d_name));
                                        done++;
                                        //fprintf(stderr,"%s\n",dl->d_name);
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

            if (done) {    // We found a filename match for the alert
                // Go draw the weather alert (kind'a)
//WE7U
                draw_map (w,
                    dir,                // Alert directory
                    alert->filename,    // Shapefile filename
                    alert,
                    -1,                 // Signifies "DON'T DRAW THE SHAPE"
                    destination_pixmap,
                    1 /* draw_filled */ );
            }
            else {      // No filename found that matches the first two
                        // characters that we already computed.

                //
                // Need code here
                //

            }
        }
        else {  // Still no filename for the weather alert.
                // Output an error message?
            //
            // Need code here
            //

        }
    }


// MAPS, not alerts

    else {  // We're doing regular maps, not weather alerts
        time_t map_timestamp;


        dm = opendir (dir);
        if (!dm) {  // Couldn't open directory
            xastir_snprintf(fullpath, sizeof(fullpath), "aprsmap %s", dir);
            if (warn)
                perror (fullpath);
        }
        else {
            int count = 0;
            while ((dl = readdir (dm))) {
                int i;

                // Check the file/directory name for control
                // characters
                for (i = 0; i < (int)strlen(dl->d_name); i++) {
                    // Dump out a warning if control characters
                    // other than LF or CR are found.
                    if ( (dl->d_name[i] != '\n')
                            && (dl->d_name[i] != '\r')
                            && (dl->d_name[i] < 0x20) ) {

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
                if (stat (fullpath, &nfile) == 0) {
                    ftime = (time_t *)&nfile.st_ctime;
                    switch (nfile.st_mode & S_IFMT) {
                        case (S_IFDIR):     // It's a directory, recurse
                            //fprintf(stderr,"file %c letter %c\n",dl->d_name[0],letter);
                            if ((strcmp (dl->d_name, ".") != 0) && (strcmp (dl->d_name, "..") != 0)) {

                                //fprintf(stderr,"FULL PATH %s\n",fullpath);

                                // If we're indexing, throw the
                                // directory into the map index as
                                // well.
                                if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
                                        || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {
                                    char temp_dir[MAX_FILENAME];

                                    // Drop off the base part of the
                                    // path for the indexing,
                                    // usually
                                    // "/usr/local/xastir/maps".
                                    // Add a '/' to the end.
                                    xastir_snprintf(temp_dir,
                                        sizeof(temp_dir),
                                        "%s/",
                                        &fullpath[map_dir_length+1]);

                                    // Add the directory to the
                                    // in-memory map index.
                                    index_update_directory(temp_dir);
                                }

                                strcpy (this_time, ctime (ftime));
                                map_search(w, fullpath, alert, alert_count, warn, destination_pixmap);
                            }
                            break;

                        case (S_IFREG):     // It's a file, draw the map
                            /*fprintf(stderr,"FILE %s\n",dl->d_name); */

                            // Get the last-modified timestamp for the map file
                            map_timestamp = (time_t)nfile.st_mtime;

                            // Check whether we're doing indexing or
                            // map drawing.  If indexing, we only
                            // want to index if the map timestamp is
                            // newer than the index timestamp.
                            if (destination_pixmap == INDEX_CHECK_TIMESTAMPS
                                    || destination_pixmap == INDEX_NO_TIMESTAMPS) {
                                // We're doing indexing, not map drawing
                                char temp_dir[MAX_FILENAME];

                                // Drop off the base part of the
                                // path for the indexing, usually
                                // "/usr/local/xastir/maps".
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
                                        && (map_timestamp < map_index_timestamp) ) {
                                    // Map is older than index _and_
                                    // we're supposed to check
                                    // timestamps.
                                    count++;
                                    break;  // Skip indexing this file
                                }
                                else {  // Map is newer or we're ignoring timestamps.
                                    // We'll index the map
                                    if (debug_level & 16) {
                                        fprintf(stderr,"Indexing map: %s\n",fullpath);
                                    }
                                }
                            }

                            // Check whether the file is in a subdirectory
                            if (strncmp (fullpath, map_dir, (size_t)map_dir_length) != 0) {
                                draw_map (w,
                                    dir,
                                    dl->d_name,
                                    alert ? &alert[*alert_count] : NULL,
                                    '\0',
                                    destination_pixmap,
                                    1 /* draw_filled */ );
                                if (alert_count && *alert_count)
                                    (*alert_count)--;
                            }
                            else {
                                // File is in the main map directory
                                // Find the '/' character
                                for (ptr = &fullpath[map_dir_length]; *ptr == '/'; ptr++) ;
                                draw_map (w,
                                    map_dir,
                                    ptr,
                                    alert ? &alert[*alert_count] : NULL,
                                    '\0',
                                    destination_pixmap,
                                    1 /* draw_filled */ );
                                if (alert_count && *alert_count)
                                    (*alert_count)--;
                            }
                            count++;
                            break;

                        default:
                            break;
                    }
                }
            }
            if (debug_level & 16)
                fprintf(stderr,"Number of maps queried: %d\n", count);

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
void free_map_index(map_index_record *index_list_head) {
    map_index_record *current;
    map_index_record *temp;


    current = index_list_head;

    while (current != NULL) {
        temp = current;
        current = current->next;
        free(temp);
    }

    index_list_head = NULL;
}





// Function to copy just the properties fields from the backup map
// index to the primary index.  Must match each record before
// copying.  Once it's done, it frees the backup map index.
//
void map_index_copy_properties(map_index_record *primary_index_head,
        map_index_record *backup_index_head) {
    map_index_record *primary;
    map_index_record *backup;


    backup = backup_index_head;

    // Walk the backup list, comparing the filename field with the
    // primary list.  When a match is found, copy just the
    // Properties fields (map_layer/draw_filled/auto_maps/selected)
    // across to the primary record.
    //
    while (backup != NULL) {
        int done = 0;

        primary = primary_index_head;

        while (!done && primary != NULL) {

            if (strcmp(primary->filename, backup->filename) == 0) { // If match

                if (debug_level & 16) {
                    fprintf(stderr,"Match: %s\t%s\n",
                        primary->filename,
                        backup->filename);
                }

                // Copy the Properties across
                primary->map_layer   = backup->map_layer;
                primary->draw_filled = backup->draw_filled;
                primary->auto_maps   = backup->auto_maps;
                primary->selected    = backup->selected;

                // Done copying this backup record.  Go on to the
                // next.  Skip the rest of the primary list for this
                // iteration.
                done++;
            }
            else {  // No match, walk the primary list looking for one.
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
void index_update_directory(char *directory) {

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
            || ( (directory[1] == '/') && (strlen(directory) == 1)) ) {
        fprintf(stderr,"index_update_directory: Bad input: %s\n",directory);
        return;
    }
    // Make sure there aren't any weird characters in the directory
    // that might cause problems later.  Look for control characters
    // and convert them to string-end characters.
    for ( i = 0; i < (int)strlen(directory); i++ ) {
        // Change any control characters to '\0' chars
        if (directory[i] < 0x20) {

            fprintf(stderr,"\nindex_update_directory: Found control char 0x%02x in map file/map directory name:\n%s\n",
                directory[i],
                directory);

            directory[i] = '\0';    // Terminate it here
        }
    }
    // Check if the string is _now_ bogus
    if ( (directory[0] == '\0')
            || (directory[strlen(directory) - 1] != '/')
            || ( (directory[1] == '/') && (strlen(directory) == 1))) {
        fprintf(stderr,"index_update_directory: Bad input: %s\n",directory);
        return;
    }

    //if (map_index_head == NULL)
    //    fprintf(stderr,"Empty list\n");

    // Search for a matching directory name in the linked list
    while ((current != NULL) && !done) {
        int test;

        //fprintf(stderr,"Comparing %s to\n          %s\n",
        //    current->filename, directory);

        test = strcmp(current->filename, directory);
        if (test == 0) {
            // Found a match!
            //fprintf(stderr,"Found: Updating entry for %s\n",directory);
            temp_record = current;
            done++; // Exit loop, "current" points to found record
        }
        else if (test > 0) {    // Found a string past us in the
                                // alphabet.  Insert ahead of this
                                // last record.

            //fprintf(stderr,"\n%s\n%s\n",current->filename,directory);

            //fprintf(stderr,"Not Found: Inserting an index record for %s\n",directory);
            temp_record = (map_index_record *)malloc(sizeof(map_index_record));

            if (current == map_index_head) {  // Start of list!
                // Insert new record at head of list
                temp_record->next = map_index_head;
                map_index_head = temp_record;
                //fprintf(stderr,"Inserting at head of list\n");
            }
            else {  // Insert between "previous" and "current"
                // Insert new record before "current"
                previous->next = temp_record;
                temp_record->next = current;
                //fprintf(stderr,"Inserting before current\n");
            }

            //fprintf(stderr,"Adding:%d:%s\n",strlen(directory),directory);
 
            // Fill in some default values for the new record.
            temp_record->selected = 0;
            temp_record->auto_maps = 1;

            //current = current->next;
            done++;
        }
        else {  // Haven't gotten to the correct insertion point yet
            previous = current; // Save ptr to last record
            current = current->next;
        }
    }

    if (!done) {    // Matching record not found, add a record to
                    // the end of the list.  "previous" points to
                    // the last record in the list or NULL (empty
                    // list).
        //fprintf(stderr,"Not Found: Adding an index record for %s\n",directory);
        temp_record = (map_index_record *)malloc(sizeof(map_index_record));
        temp_record->next = NULL;

        if (previous == NULL) { // Empty list
            map_index_head = temp_record;
            //fprintf(stderr,"First record in new list\n");
        }
        else {  // Else at end of list
            previous->next = temp_record;
            //fprintf(stderr,"Adding to end of list: %s\n",directory);
        }

        //fprintf(stderr,"Adding:%d:%s\n",strlen(directory),directory);
 
        // Fill in some default values for the new record.
        temp_record->selected = 0;
        temp_record->auto_maps = 1;
    }

    // Update the values.  By this point we have a struct to fill
    // in, whether it's a new or old struct doesn't matter.  Convert
    // the values from lat/long to Xastir coordinate system.
    xastir_snprintf(temp_record->filename,MAX_FILENAME,"%s",directory);
    //strncpy(temp_record->filename,directory,MAX_FILENAME-1);
    //temp_record->filename[MAX_FILENAME-1] = '\0';
//    xastir_snprintf(temp_record->filename,strlen(temp_record->filename),"%s",directory);

    temp_record->bottom = 0;
    temp_record->top = 0;
    temp_record->left = 0;
    temp_record->right = 0;
    temp_record->accessed = 1;
    temp_record->map_layer = 0;
    temp_record->draw_filled = 0;
}





// Function called by the various draw_* functions when in indexing
// mode.  Causes an update of the index list in memory.  Input
// parameters are in the Xastir coordinate system due to speed
// considerations.  Records are inserted in alphanumerical order.
void index_update_xastir(char *filename,
                  unsigned long bottom,
                  unsigned long top,
                  unsigned long left,
                  unsigned long right) {

    map_index_record *current = map_index_head;
    map_index_record *previous = map_index_head;
    map_index_record *temp_record = NULL;
    int done = 0;
    int i;


    // Check for initial bad input
    if ( (filename == NULL)
            || (filename[0] == '\0')
            || (filename[strlen(filename) - 1] == '/') ) {
        fprintf(stderr,"index_update_xastir: Bad input: %s\n",filename);
        return;
    }
    // Make sure there aren't any weird characters in the filename
    // that might cause problems later.  Look for control characters
    // and convert them to string-end characters.
    for ( i = 0; i < (int)strlen(filename); i++ ) {
        // Change any control characters to '\0' chars
        if (filename[i] < 0x20) {

            fprintf(stderr,"\nindex_update_xastir: Found control char 0x%02x in map file/map directory name:\n%s\n",
                filename[i],
                filename);

            filename[i] = '\0';    // Terminate it here
        }
    }
    // Check if the string is _now_ bogus
    if (filename[0] == '\0') {
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
            || strstr(filename,"DBF") ) {
        return;
    }

    // Search for a matching filename in the linked list
    while ((current != NULL) && !done) {
        int test;

        //fprintf(stderr,"Comparing %s to\n          %s\n",current->filename,filename);

        test = strcmp(current->filename,filename);
        if (test == 0) {
            // Found a match!
            //fprintf(stderr,"Found: Updating entry for %s\n",filename);
            temp_record = current;
            done++; // Exit the while loop
        }
        else if (test > 0) {    // Found a string past us in the
                                // alphabet.  Insert ahead of this
                                // last record.

            //fprintf(stderr,"\n%s\n%s\n",current->filename,filename);

            //fprintf(stderr,"Not Found: Inserting an index record for %s\n",filename);
            temp_record = (map_index_record *)malloc(sizeof(map_index_record));

            if (current == map_index_head) {  // Start of list!
                // Insert new record at head of list
                temp_record->next = map_index_head;
                map_index_head = temp_record;
                //fprintf(stderr,"Inserting at head of list\n");
            }
            else {
                // Insert new record before "current"
                previous->next = temp_record;
                temp_record->next = current;
                //fprintf(stderr,"Inserting before current\n");
            }

            //fprintf(stderr,"Adding:%d:%s\n",strlen(filename),filename);

            // Fill in some default values for the new record
//WE7U
// Here's where we might look at the file extension and assign
// default map_layer/draw_filled fields based on that.
            temp_record->map_layer = 0;
            temp_record->draw_filled = 0;
            temp_record->selected = 0;
            temp_record->auto_maps = 1;
        
            //current = current->next;
            done++;
        }
        else {  // Haven't gotten to the correct insertion point yet
            previous = current; // Save ptr to last record
            current = current->next;
        }
    }

    if (!done) {  // Matching record not found, add a
        // record to the end of the list
        //fprintf(stderr,"Not Found: Adding an index record for %s\n",filename);
        temp_record = (map_index_record *)malloc(sizeof(map_index_record));
        temp_record->next = NULL;

        if (previous == NULL) { // Empty list
            map_index_head = temp_record;
            //fprintf(stderr,"First record in new list\n");
        }
        else {  // Else at end of list
            previous->next = temp_record;
            //fprintf(stderr,"Adding to end of list: %s\n",filename);
        }

        //fprintf(stderr,"Adding:%d:%s\n",strlen(filename),filename);

        // Fill in some default values for the new record
//WE7U
// Here's where we might look at the file extension and assign
// default map_layer/draw_filled fields based on that.
        temp_record->map_layer = 0;
        temp_record->draw_filled = 0;
        temp_record->selected = 0;
        temp_record->auto_maps= 1;
    }

    // Update the values.  By this point we have a struct to fill
    // in, whether it's a new or old struct doesn't matter.  Convert
    // the values from lat/long to Xastir coordinate system.
    xastir_snprintf(temp_record->filename,MAX_FILENAME,"%s",filename);
    //strncpy(temp_record->filename,filename,MAX_FILENAME-1);
    //temp_record->filename[MAX_FILENAME-1] = '\0';
//    xastir_snprintf(temp_record->filename,strlen(temp_record->filename),"%s",filename);

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
                  double right) {

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
            || (filename[strlen(filename) - 1] == '/') ) {
        fprintf(stderr,"index_update_ll: Bad input: %s\n",filename);
        return;
    }
    // Make sure there aren't any weird characters in the filename
    // that might cause problems later.  Look for control characters
    // and convert them to string-end characters.
    for ( i = 0; i < (int)strlen(filename); i++ ) {
        // Change any control characters to '\0' chars
        if (filename[i] < 0x20) {

            fprintf(stderr,"\nindex_update_ll: Found control char 0x%02x in map file/map directory name:\n%s\n",
                filename[i],
                filename);

            filename[i] = '\0';    // Terminate it here
        }
    }
    // Check if the string is _now_ bogus
    if (filename[0] == '\0') {
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
            || strstr(filename,"DBF") ) {
        return;
    }

    // Search for a matching filename in the linked list
    while ((current != NULL) && !done) {
        int test;

        //fprintf(stderr,"Comparing %s to\n          %s\n",current->filename,filename);

        test = strcmp(current->filename,filename);

        if (test == 0) {
            // Found a match!
            //fprintf(stderr,"Found: Updating entry for %s\n",filename);
            temp_record = current;
            done++; // Exit the while loop
        }

        else if (test > 0) {
            // Found a string past us in the alphabet.  Insert ahead
            // of this last record.

            //fprintf(stderr,"\n%s\n%s\n",current->filename,filename);

            //fprintf(stderr,"Not Found: Inserting an index record for %s\n",filename);
            temp_record = (map_index_record *)malloc(sizeof(map_index_record));

            if (current == map_index_head) {  // Start of list!
                // Insert new record at head of list
                temp_record->next = map_index_head;
                map_index_head = temp_record;
                //fprintf(stderr,"Inserting at head of list\n");
            }
            else {
                // Insert new record before "current"
                previous->next = temp_record;
                temp_record->next = current;
                //fprintf(stderr,"Inserting before current\n");
            }

            //fprintf(stderr,"Adding:%d:%s\n",strlen(filename),filename);

            // Fill in some default values for the new record
//WE7U
// Here's where we might look at the file extension and assign
// default map_layer/draw_filled fields based on that.
            temp_record->map_layer = 0;
            temp_record->draw_filled = 0;
            temp_record->selected = 0;
            temp_record->auto_maps = 1;
 
            //current = current->next;
            done++;
        }
        else {  // Haven't gotten to the correct insertion point yet
            previous = current; // Save ptr to last record
            current = current->next;
        }
    }

    if (!done) {    // Matching record not found, didn't find alpha
                    // chars after our string either, add record to
                    // the end of the list.

        //fprintf(stderr,"Not Found: Adding an index record for %s\n",filename);
        temp_record = (map_index_record *)malloc(sizeof(map_index_record));
        temp_record->next = NULL;

        if (previous == NULL) { // Empty list
            map_index_head = temp_record;
            //fprintf(stderr,"First record in new list\n");
        }
        else {  // Else at end of list
            previous->next = temp_record;
            //fprintf(stderr,"Adding to end of list: %s\n",filename);
        }

        //fprintf(stderr,"Adding:%d:%s\n",strlen(filename),filename);

        // Fill in some default values for the new record
//WE7U
// Here's where we might look at the file extension and assign
// default map_layer/draw_filled fields based on that.
        temp_record->map_layer = 0;
        temp_record->draw_filled = 0;
        temp_record->selected = 0;
        temp_record->auto_maps = 1;
    }

    // Update the values.  By this point we have a struct to fill
    // in, whether it's a new or old struct doesn't matter.  Convert
    // the values from lat/long to Xastir coordinate system.

    // In this case the struct uses MAX_FILENAME for the length of
    // the field, so the below statement is ok.
    xastir_snprintf(temp_record->filename,MAX_FILENAME,"%s",filename);

    //strncpy(temp_record->filename,filename,MAX_FILENAME-1);
    //temp_record->filename[MAX_FILENAME-1] = '\0';
//    xastir_snprintf(temp_record->filename,strlen(temp_record->filename),"%s",filename);

    ok = convert_to_xastir_coordinates( &temp_left,
        &temp_top,
        (float)left,
        (float)top);
    if (!ok)
        fprintf(stderr,"%s\n\n",filename);

    ok = convert_to_xastir_coordinates( &temp_right,
        &temp_bottom,
        (float)right,
        (float)bottom);
    if (!ok)
        fprintf(stderr,"%s\n\n",filename);
 
    temp_record->bottom = temp_bottom;
    temp_record->top = temp_top;
    temp_record->left = temp_left;
    temp_record->right = temp_right;
    temp_record->accessed = 1;
}





// Function which will update the "accessed" variable on either a
// directory or a filename in the map index.
void index_update_accessed(char *filename) {
    map_index_record *current = map_index_head;
    int done = 0;
    int i;


    // Check for initial bad input
    if ( (filename == NULL) || (filename[0] == '\0') ) {
        fprintf(stderr,"index_update_accessed: Bad input: %s\n",filename);
        return;
    }

    // Make sure there aren't any weird characters in the filename
    // that might cause problems later.  Look for control characters
    // and convert them to string-end characters.
    for ( i = 0; i < (int)strlen(filename); i++ ) {
        // Change any control characters to '\0' chars
        if (filename[i] < 0x20) {

            fprintf(stderr,"\nindex_update_accessed: Found control char 0x%02x in map file/map directory name:\n%s\n",
                filename[i],
                filename);

            filename[i] = '\0';    // Terminate it here
        }
    }
    // Check if the string is _now_ bogus
    if (filename[0] == '\0') {
        fprintf(stderr,"index_update_accessed: Bad input: %s\n",filename);
        return;
    }

    // Skip dbf and shx map extensions.  Really should make this
    // case-independent...
    if (       strstr(filename,"shx")
            || strstr(filename,"dbf")
            || strstr(filename,"SHX")
            || strstr(filename,"DBF") ) {
        return;
    }

    // Search for a matching filename in the linked list
    while ((current != NULL) && !done) {
        int test;

//fprintf(stderr,"Comparing %s to\n          %s\n",current->filename,filename);

        test = strcmp(current->filename,filename);

        if (test == 0) {
            // Found a match!
//fprintf(stderr,"Found: Updating entry for %s\n\n",filename);
            current->accessed = 1;
            done++; // Exit the while loop
        }

        else {  // Haven't gotten to the correct insertion point yet
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
// It'll speed things up a bit.  Make this change sometime soon.
//
int index_retrieve(char *filename,
                   unsigned long *bottom,
                   unsigned long *top,
                   unsigned long *left,
                   unsigned long *right,
                   int *map_layer,
                   int *draw_filled,
                   int *auto_maps) {

    map_index_record *current = map_index_head;
    int status = 0;

    if (filename == NULL) {
        return(status);
    }

    // Search for a matching filename in the linked list
    while ( (current != NULL)
            && (strlen(filename) < MAX_FILENAME) ) {

        if (strcmp(current->filename,filename) == 0) {
            // Found a match!
            status = 1;
            *bottom = current->bottom;
            *top = current->top;
            *left = current->left;
            *right = current->right;
            *map_layer = current->map_layer;
            *draw_filled = current->draw_filled;
            *auto_maps = current->auto_maps;
            break;  // Exit the while loop
        }
        else {
            current = current->next;
        }
    }
    return(status);
}





// Saves the linked list pointed to by map_index_head to a file.
// Keeps the same order as the memory linked list.  Delete records
// in the in-memory linked list for which the "accessed" variable is
// 0 or filename is empty.
//
void index_save_to_file() {
    FILE *f;
    map_index_record *current;
    map_index_record *last;
    char out_string[MAX_FILENAME*2];


//fprintf(stderr,"Saving map index to file\n");

    f = fopen(MAP_INDEX_DATA,"w");

    if (f == NULL) {
        fprintf(stderr,"Couldn't create/update map index file: %s\n",
            MAP_INDEX_DATA);
        return;
    }

    current = map_index_head;
    last = current;

    while (current != NULL) {
        int i;

        // Make sure there aren't any weird characters in the
        // filename that might cause problems later.  Look for
        // control characters and convert them to string-end
        // characters.
        for ( i = 0; i < (int)strlen(current->filename); i++ ) {
            // Change any control characters to '\0' chars
            if (current->filename[i] < 0x20) {

                fprintf(stderr,"\nindex_save_to_file: Found control char 0x%02x in map name:\n%s\n",
                    current->filename[i],
                    current->filename);

                current->filename[i] = '\0';    // Terminate it here
            }
        }
 
        // Save to file if filename non-blank and record has the
        // accessed field set.
        if ( (current->filename[0] != '\0')
                && (current->accessed != 0) ) {

            // Write each object out to the file as one
            // comma-delimited line
            xastir_snprintf(out_string,
                sizeof(out_string),
                "%010lu,%010lu,%010lu,%010lu,%05d,%01d,%01d,%s\n",
                current->bottom,
                current->top,
                current->left,
                current->right,
                current->map_layer,
                current->draw_filled,
                current->auto_maps,
                current->filename);

            if (fprintf(f,"%s",out_string) < (int)strlen(out_string)) {
                // Failed to write
                fprintf(stderr,"Couldn't write objects to map index file: %s\n",
                    MAP_INDEX_DATA);
                current = NULL; // All done
            }
            // Set up pointers for next loop iteration
            last = current;
            current = current->next;
        }


        else {
            last = current;
            current = current->next;
        }
/*
//WE7U
        else {  // Delete this record from our list!  It's a record
                // for a map file that doesn't exist in the
                // filesystem anymore.
            if (last == current) {   // We're at the head of the list
                map_index_head = current->next;
                free(current);

                // Set up pointers for next loop iteration
                current = map_index_head;
                last = current;
            }
            else {  // Not the first record in the list
                map_index_record *gone;

                gone = current; // Save ptr to record we wish to delete 
                last->next = current->next; // Unlink from list
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





//WE7U2
// Function used to add map directories/files to the in-memory map
// index.  Causes an update of the index list in memory.  Input
// records are inserted in alphanumerical order.  This function is
// called from the index_restore_from_file() function below.  When
// this function is called the new record has all of the needed
// information in it.
//
void index_insert_sorted(map_index_record *new_record) {

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
            current->map_layer = new_record->map_layer;
            current->draw_filled = new_record->draw_filled;
            current->selected = selected;   // Restore it
            current->auto_maps = new_record->auto_maps;

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
            new_record->auto_maps = 1;

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
        new_record->auto_maps = 1;
    }
}





// sort map index
// simple bubble sort, since we should be sorted already
//
void index_sort(void) {
    map_index_record *current, *previous, *next;
    int changed = 1;
    int loops = 0; // for debug stats

    previous = map_index_head;
    next = NULL;
    //  fprintf(stderr, "index_sort: start.\n");
    // check if we have any records at all, and at least two
    if ( (previous != NULL) && (previous->next != NULL) ) {
        current = previous->next;
        while ( changed == 1) {
            changed = 0;
            if (current->next != NULL) {next = current->next;}
            if ( strcmp( previous->filename, current->filename) >= 0 ) {
                // out of order - swap them
                current->next = previous;
                previous->next = next;
                map_index_head = current;
                current = previous;
                previous = map_index_head;
                changed = 1;
            }
            
            while ( next != NULL ) {
                if ( strcmp( current->filename, next->filename) >= 0 ) {
                    // out of order - swap them
                    current->next = next->next;
                    previous->next = next;
                    next->next = current;
                    // get ready for the next iteration
                    previous = next;  // current already moved ahead from the swap
                    next = current->next;
                    changed = 1;
                } else {
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





//WE7U2
// Snags the file and creates the linked list pointed to by the
// map_index_head pointer.  The memory linked list keeps the same
// order as the entries in the file.
//
void index_restore_from_file(void) {
    FILE *f;
    map_index_record *temp_record;
    map_index_record *last_record;
    char in_string[MAX_FILENAME*2];

//fprintf(stderr,"\nRestoring map index from file\n");

    if (map_index_head != NULL) {
        fprintf(stderr,"Warning: index_restore_from_file(): map_index_head was non-null!\n");
    }

    map_index_head = NULL;  // Starting with empty list
    last_record = NULL;

    f = fopen(MAP_INDEX_DATA,"r");
    if (f == NULL)  // No map_index file yet
        return;

    while (!feof (f)) { // Loop through entire map_index file

        // Read one line from the file
        if ( get_line (f, in_string, MAX_FILENAME*2) ) {

            if (strlen(in_string) >= 15) {   // We have some data.
                                            // Try to process the
                                            // line.
                char scanf_format[50];
                int processed;
                int i;

//fprintf(stderr,"%s\n",in_string);

                // Tweaked the string below so that it will track
                // along with MAX_FILENAME-1.  We're constructing
                // the string "%lu,%lu,%lu,%lu,%d,%d,%2000c", where
                // the 2000 example number is from MAX_FILENAME.
                xastir_snprintf(scanf_format,
                    sizeof(scanf_format),
                    "%s%d%s",
                    "%lu,%lu,%lu,%lu,%d,%d,%d,%",
                    MAX_FILENAME,
                    "c");
                //fprintf(stderr,"%s\n",scanf_format);

                // Malloc an index record.  We'll add it to the list
                // only if the data looks reasonable.
                temp_record = (map_index_record *)malloc(sizeof(map_index_record));
                memset(temp_record->filename, 0, sizeof(temp_record->filename));
                temp_record->next = NULL;

                processed = sscanf(in_string,
                    scanf_format,
                    &temp_record->bottom,
                    &temp_record->top,
                    &temp_record->left,
                    &temp_record->right,
                    &temp_record->map_layer,
                    &temp_record->draw_filled,
                    &temp_record->auto_maps,
                    temp_record->filename);

                // Do some reasonableness checking on the parameters
                // we just parsed.
//WE7U: Comparison is always false
                if ( (temp_record->bottom < 0l)
                        || (temp_record->bottom > 64800000l) ) {
                    processed = 0;  // Reject this record
                    fprintf(stderr,"\nindex_restore_from_file: bottom extent incorrect %lu in map name:\n%s\n",
                            temp_record->bottom,
                            temp_record->filename);
                }

 
//WE7U: Comparison is always false
               if ( (temp_record->top < 0l)
                        || (temp_record->top > 64800000l) ) {
                    processed = 0;  // Reject this record
                    fprintf(stderr,"\nindex_restore_from_file: top extent incorrect %lu in map name:\n%s\n",
                            temp_record->top,
                            temp_record->filename);
                }

//WE7U: Comparison is always false
                if ( (temp_record->left < 0l)
                        || (temp_record->left > 129600000l) ) {
                    processed = 0;  // Reject this record
                    fprintf(stderr,"\nindex_restore_from_file: left extent incorrect %lu in map name:\n%s\n",
                            temp_record->left,
                            temp_record->filename);
                }

//WE7U: Comparison is always false
                if ( (temp_record->right < 0l)
                        || (temp_record->right > 129600000l) ) {
                    processed = 0;  // Reject this record
                    fprintf(stderr,"\nindex_restore_from_file: right extent incorrect %lu in map name:\n%s\n",
                            temp_record->right,
                            temp_record->filename);
                }

                if ( (temp_record->map_layer < 0)
                        || (temp_record->map_layer > 99999) ) {
                    processed = 0;  // Reject this record
                    fprintf(stderr,"\nindex_restore_from_file: map_layer field incorrect %d in map name:\n%s\n",
                            temp_record->map_layer,
                            temp_record->filename);
                }

                if ( (temp_record->draw_filled < 0)
                        || (temp_record->draw_filled > 1) ) {
                    processed = 0;  // Reject this record
                    fprintf(stderr,"\nindex_restore_from_file: draw_filled field incorrect %d in map name:\n%s\n",
                            temp_record->draw_filled,
                            temp_record->filename);
                }

                if ( (temp_record->auto_maps < 0)
                        || (temp_record->auto_maps > 1) ) {
                    processed = 0;  // Reject this record
                    fprintf(stderr,"\nindex_restore_from_file: auto_maps field incorrect %d in map name:\n%s\n",
                            temp_record->auto_maps,
                            temp_record->filename);
                }

                // Check whether the filename is empty
                if (strlen(temp_record->filename) == 0) {
                    processed = 0;  // Reject this record
                }

                // Check for control characters in the filename.
                // Reject any that have them.
                for (i = 0; i < (int)strlen(temp_record->filename); i++)
                {
                    if (temp_record->filename[i] < 0x20) {

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

                // If correct number of parameters
                if (processed == 8) {

                    //fprintf(stderr,"Restored: %s\n",temp_record->filename);
 
                    // Insert the new record into the in-memory map
                    // list in sorted order.
                    // --slow for large lists
                    // index_insert_sorted(temp_record);
                    // -- so we just add it to the end of the list
                    // and sort it at the end tp make sure nobody 
                    // messed us up by editting the file by hand
                    if ( last_record == NULL ) { // first record
                        map_index_head = temp_record;
                    } else {
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
                else {  // sscanf didn't parse the proper number of
                        // items.  Delete the record.
                    free(temp_record);
                }
            }
        }
    }
    (void)fclose(f);
    // now that we have read the whole file, make sure it is sorted
    index_sort();  // probably should check for dup records
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
void map_indexer(int parameter) {
    struct stat nfile;
    int check_times = 1;
    FILE *f;
    map_index_record *current;
    map_index_record *backup_list_head = NULL;


    if (debug_level & 16)
        fprintf(stderr,"map_indexer() start\n");


    // Find the timestamp on the index file first.  Save it away so
    // that the timestamp for each map file can be compared to it.
    if (stat (MAP_INDEX_DATA, &nfile) != 0) {

        // File doesn't exist yet.  Create it.
        f = fopen(MAP_INDEX_DATA,"w");
        if (f != NULL)
            (void)fclose(f);
        else
            fprintf(stderr,"Couldn't create map index file: %s\n", MAP_INDEX_DATA);
        
        check_times = 0; // Don't check the timestamps.  Do them all. 
    }
    else {  // File exists
        map_index_timestamp = (time_t)nfile.st_mtime;
        check_times = 1;
    }


    if (parameter == 1) {   // Full indexing instead of timestamp-check indexing

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
    while (current != NULL) {
        current->accessed = 0;
        current = current->next;
    }


    if (check_times)
        map_search (NULL, AUTO_MAP_DIR, NULL, NULL, (int)FALSE, INDEX_CHECK_TIMESTAMPS);
    else
        map_search (NULL, AUTO_MAP_DIR, NULL, NULL, (int)FALSE, INDEX_NO_TIMESTAMPS);

    if (debug_level & 16)
        fprintf(stderr,"map_indexer() middle\n");


    if (parameter == 1) {   // Full indexing instead of timestamp-check indexing
        // Copy the Properties from the backup list to the new list,
        // then free the backup list.
        map_index_copy_properties(map_index_head, backup_list_head);
    }

 
    // Save the updated index to the file
    index_save_to_file();

    if (debug_level & 16)
        fprintf(stderr,"map_indexer() end\n");
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
void fill_in_new_alert_entries(Widget w, char *dir) {
    int ii;
    char alert_scan[MAX_FILENAME], *dir_ptr;


    alert_count = MAX_ALERT - 1;

    // Set up our path to the wx alert maps
    memset (alert_scan, 0, sizeof (alert_scan));    // Zero our alert_scan string
    strncpy (alert_scan, dir, MAX_FILENAME-10); // Fetch the base directory
    strcat (alert_scan, "/");   // Complete alert directory is now set up in the string
    dir_ptr = &alert_scan[strlen (alert_scan)]; // Point to end of path

    // Iterate through the weather alerts we currently have on
    // our list.  It looks like we wish to just fill in the
    // alert struct and to determine whether the alert is within
    // our viewport here.  We don't really wish to draw the
    // alerts at this stage, that comes just a bit later in this
    // routine.
    for (ii = 0; ii < alert_max_count; ii++) {

        // Check whether alert slot is empty/filled
        if (alert_list[ii].title[0] == '\0') {  // It's empty,
            // skip this iteration of the loop and advance to
            // the next slot.
            continue;
        }
        else if (!alert_list[ii].filename[0]) { // Filename is
            // empty, we need to fill it in.

            //fprintf(stderr,"load_alert_maps() Title: %s\n",alert_list[ii].title);

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
            map_search (w,
                alert_scan,
                &alert_list[ii],
                &alert_count,
                (int)alert_list[ii].flags[1],
                DRAW_TO_PIXMAP_ALERTS);

            //fprintf(stderr,"Title1:%s\n",alert_list[ii].title);
        }
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
void load_alert_maps (Widget w, char *dir) {
    int ii, level;
    unsigned char fill_color[] = {  (unsigned char)0x69,    // gray86
                                    (unsigned char)0x4a,    // red2
                                    (unsigned char)0x63,    // yellow2
                                    (unsigned char)0x66,    // cyan2
                                    (unsigned char)0x61,    // RoyalBlue
                                    (unsigned char)0x64,    // ForestGreen
                                    (unsigned char)0x62 };  // orange3


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
// Actually, since the alert_list isn't currently ordered at all,
// perhaps we need to order it by priority, then by map file, so
// that we can draw the shapes from each map file in the correct
// order.  This might cause each map file to be drawn up to three
// times (once for each priority level), but that's better than
// calling each map for each zone as is done now.


    for (ii = alert_max_count - 1; ii >= 0; ii--) {

        //  Check whether the alert slot is filled/empty
        if (alert_list[ii].title[0] == '\0') // Empty slot
            continue;

//        if (alert_list[ii].flags[0] == 'Y' && (level = alert_active (&alert_list[ii], ALERT_ALL))) {
        if ( (level = alert_active(&alert_list[ii], ALERT_ALL) ) ) {
            if (level >= (int)sizeof (fill_color))
                level = 0;

            // The last parameter denotes drawing into pixmap_alert
            // instead of pixmap or pixmap_final.

            //fprintf(stderr,"Drawing %s\n",alert_list[ii].filename);
            //fprintf(stderr,"Title4:%s\n",alert_list[ii].title);

            // Attempt to draw alert
            if ( (alert_list[ii].alert_level != 'C')             // Alert not cancelled
                    && (alert_list[ii].index != -1) ) {          // Shape found in shapefile

                if (map_visible_lat_lon(alert_list[ii].bottom_boundary, // Shape visible
                        alert_list[ii].top_boundary,
                        alert_list[ii].left_boundary,
                        alert_list[ii].right_boundary,
                        alert_list[ii].title) ) {    // Error text if failure

                    draw_map (w,
                        dir,
                        alert_list[ii].filename,
                        &alert_list[ii],
                        fill_color[level],
                        DRAW_TO_PIXMAP_ALERTS,
                        1 /* draw_filled */ );
                }
                else {
                    //fprintf(stderr,"Alert not visible\n");
                }
            }
            else {
                // Cancelled alert, can't find the shapefile, or not
                // in our viewport, don't draw it!
                //fprintf(stderr,"Alert cancelled or shape not found\n");
            }
        }
    }

    //fprintf(stderr,"Done drawing all active alerts\n");

    if (alert_display_request()) {
        alert_redraw_on_update = redraw_on_new_data = 2;
    }
}





// Here's the head of our sorted-by-layer maps list
static map_index_record *map_sorted_list_head = NULL;


void empty_map_sorted_list(void) {
    map_index_record *current = map_sorted_list_head;

    while (map_sorted_list_head != NULL) {
        current = map_sorted_list_head;
        map_sorted_list_head = current->next;
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
void insert_map_sorted(char *filename){
    map_index_record *current;
    map_index_record *last;
    map_index_record *temp_record;
    unsigned long bottom;
    unsigned long top;
    unsigned long left;
    unsigned long right;
    int map_layer;
    int draw_filled;
    int auto_maps;
    int done;


    if (index_retrieve(filename,
            &bottom,
            &top,
            &left,
            &right,
            &map_layer,
            &draw_filled,
            &auto_maps)) {    // Found a match

        // Allocate a new record
        temp_record = (map_index_record *)malloc(sizeof(map_index_record));

        // Fill in the values
        xastir_snprintf(temp_record->filename,MAX_FILENAME,"%s",filename);
        temp_record->bottom = bottom;
        temp_record->top = top;
        temp_record->left = left;
        temp_record->right = right;
        temp_record->map_layer = map_layer;
        temp_record->draw_filled = draw_filled;
        temp_record->auto_maps = auto_maps;
        temp_record->selected = 1;  // Always, we already know this!
        temp_record->accessed = 0;
        temp_record->next = NULL;

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

        if (map_sorted_list_head == NULL) {
            // Empty list.  Insert record.
            map_sorted_list_head = temp_record;
            done++;
        }
        else if (map_layer < current->map_layer) {
            // Insert at beginning of list
            temp_record->next = current;
            map_sorted_list_head = temp_record;
            done++;
        }
        else {  // Need to insert between records or at end of list
            while (!done && (current != NULL) ) {
                if (map_layer >= current->map_layer) {  // Not to our layer yet
                    last = current;
                    current = current->next;    // May point to NULL now
                }
                else if (map_layer < current->map_layer) {
                    temp_record->next = current;
                    last->next = temp_record;
                    done++;
                }
            }
        }
        // Handle running off the end of the list
        if (!done && (current == NULL) ) {
            last->next = temp_record;
        }
    }
    else {
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
void load_auto_maps (Widget w, char *dir) {
    map_index_record *current = map_index_head;


    // Skip the sorting of the maps if we don't need to do it
    if (re_sort_maps) {

        //fprintf(stderr,"*** Sorting the selected maps by layer...\n");

        // Empty the sorted list first.  We'll create a new one.
        empty_map_sorted_list();

        // Run through the entire map_index linked list
        while (current != NULL) {

            // I included GNIS here at this time because the files are
            // very large (at least for a state-wide file), and they
            // take a long time to load.  They're obviously not a raster
            // format file.
            if (auto_maps_skip_raster
                    && (   strstr(current->filename,"geo")
                        || strstr(current->filename,"GEO")
                        || strstr(current->filename,"tif")
                        || strstr(current->filename,"TIF")
                        || strstr(current->filename,"gnis")
                        || strstr(current->filename,"GNIS") ) ) {
                // Skip this map
            }
            else {  // Draw this map

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
    while  (current != NULL) {

        // Debug
//        fprintf(stderr,"Drawing level:%05d, file:%s\n",
//            current->map_layer,
//            current->filename);

        // Draw the maps in sorted-by-layer order
        if (current->auto_maps) {
            draw_map (w,
                SELECTED_MAP_DIR,
                current->filename,
                NULL,
                '\0',
                DRAW_TO_PIXMAP,
                current->draw_filled);
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
void load_maps (Widget w) {
    FILE *f;
    char mapname[MAX_FILENAME];
    int i;
    char selected_dir[MAX_FILENAME];
    map_index_record *current;


    if (debug_level & 16)
        fprintf(stderr,"Load maps start\n");

    // Skip the sorting of the maps if we don't need to do it
    if (re_sort_maps) {

        //fprintf(stderr,"*** Sorting the selected maps by layer...\n");

        // Empty the sorted list first.  We'll create a new one.
        empty_map_sorted_list();
 
        // Make sure the string is empty before we start
        selected_dir[0] = '\0';

        // Create empty file if it doesn't exist
        (void)filecreate(SELECTED_MAP_DATA);

        f = fopen (SELECTED_MAP_DATA, "r");
        if (f != NULL) {
            if (debug_level & 16)
                fprintf(stderr,"Load maps Open map file\n");

            while (!feof (f)) {
                // Grab one line from the file
                if ( fgets( mapname, MAX_FILENAME-1, f ) != NULL ) {

                    // Forced termination (just in case)
                    mapname[MAX_FILENAME-1] = '\0';

                    // Get rid of the newline at the end
                    for (i = strlen(mapname); i > 0; i--) {
                        if (mapname[i] == '\n')
                            mapname[i] = '\0'; 
                    }

                    if (debug_level & 16)
                        fprintf(stderr,"Found mapname: %s\n", mapname);

                    // Test for comment
                    if (mapname[0] != '#') {


                        // Check whether it's a directory that was
                        // selected.  If so, save it in a special
                        // variable and use that to match all the files
                        // inside the directory.  Note that with the way
                        // we have things ordered in the list, the
                        // directories appear before their member files.
                        if (mapname[strlen(mapname)-1] == '/') {
                            // Found a directory.  Save the name.
                            xastir_snprintf(selected_dir,
                                sizeof(selected_dir),
                                "%s",
                                mapname);

//fprintf(stderr,"Selected %s directory\n",selected_dir);

                            // Here we need to run through the map_index
                            // list to find all maps that match the
                            // currently selected directory.  Attempt to
                            // load all of those maps as well.

//fprintf(stderr,"Load all maps under this directory: %s\n",selected_dir);

                            // Point to the start of the map_index list
                            current = map_index_head;

                            while (current != NULL) {

                               if (strstr(current->filename,selected_dir)) {

                                    if (current->filename[strlen(current->filename)-1] != '/') {

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
                        else { 
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
                                fprintf(stderr,"Load maps -%s\n", mapname);

                            XmUpdateDisplay (da);
                        }
                    }
                }
                else {  // We've hit EOF
                    break;
                }

            }
            (void)fclose (f);
            statusline(" ",1);      // delete status line
        }
        else
            fprintf(stderr,"Couldn't open file: %s\n", SELECTED_MAP_DATA);

        // All done sorting until something is changed in the Map
        // Chooser.
        re_sort_maps = 0;

        //fprintf(stderr,"*** DONE sorting the selected maps.\n");
    }

    // We have the maps in sorted order.  Run through the list and
    // draw them.
    current = map_sorted_list_head;
    while  (current != NULL) {

        // Debug
//        fprintf(stderr,"Drawing level:%05d, file:%s\n",
//            current->map_layer,
//            current->filename);

        // Draw the maps in sorted-by-layer order
        draw_map (w,
            SELECTED_MAP_DIR,
            current->filename,
            NULL,
            '\0',
            DRAW_TO_PIXMAP,
            current->draw_filled);
 

        current = current->next;
    }

    if (debug_level & 16)
        fprintf(stderr,"Load maps stop\n");
}
