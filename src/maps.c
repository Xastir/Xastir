/* -*- c-basic-indent: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000,2001,2002  The Xastir Group
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


#include "config.h"
#include "snprintf.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>

#ifdef USING_SOLARIS
#include <strings.h>
#endif

#ifdef HAVE_IMAGEMAGICK
#include <time.h>
#undef RETSIGTYPE
#include <magick/api.h>
#endif // HAVE_IMAGEMAGICK

#include <dirent.h>
#include <netinet/in.h>
#include <Xm/XmAll.h>

#ifdef USING_SOLARIS
  #ifndef NO_XPM
    #include <Xm/XpmI.h>
  #endif
#else
#include <X11/xpm.h>
#endif

#include <X11/Xlib.h>

#include <math.h>

#ifdef HAVE_GEOTIFF
//#include "cpl_csv.h"
#include "xtiffio.h"
#include "geotiffio.h"
#include "geo_normalize.h"
#include "projects.h"
#endif // HAVE_GEOTIFF

#ifdef HAVE_SHAPELIB
#include "libshp/shapefil.h"
#endif  // HAVE_SHAPELIB

#include "xastir.h"
#include "maps.h"
#include "alert.h"
#include "util.h"
#include "main.h"
#include "datum.h"
#include "draw_symbols.h"
#include "rotated.h"

#define DOS_HDR_LINES 8
#define GRID_MORE 5000


#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }


// Check for XPM and/or ImageMagick.  We use "NO_GRAPHICS"
// to disable some routines below if the support for them
// is not compiled in.
#ifdef NO_XPM
  #ifndef HAVE_IMAGEMAGICK
    #define NO_GRAPHICS 1
  #endif
#endif


//WE7U4
// Print options
Widget print_properties_dialog = (Widget)NULL;
static xastir_mutex print_properties_dialog_lock;
Widget rotate_90 = (Widget)NULL;
Widget auto_rotate = (Widget)NULL;
char  print_paper_size[20] = "Letter";  // Displayed in dialog, but not used yet.
int   print_rotated = 0;
int   print_auto_rotation = 0;
float print_scale = 1.0;                // Not used yet.
int   print_auto_scale = 0;
int   print_blank_background_color = 0; // Not used yet.
int   print_in_monochrome = 0;
int   print_resolution = 150;           // 72 dpi is normal for Postscript.
                                        // 100 or 150 dpi work well with HP printer
int   print_invert = 0;                 // Reverses black/white

time_t last_snapshot = 0;               // Used to determine when to take next snapshot



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

float geotiff_map_intensity = 0.65;  // Map color intensity, set from Maps->Map Intensity





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
    
    //printf("Scale: x %5.3fkm/deg, y %5.3fkm/deg, x %ld y %ld\n",sc_x*360,sc_y*360,xsc,ysc);
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
void convert_to_xastir_coordinates ( unsigned long* x,
                                    unsigned long* y,
                                    float f_longitude,
                                    float f_latitude )
{
    *y = (unsigned long)(32400000l + (360000.0 * (-f_latitude)));
    *x = (unsigned long)(64800000l + (360000.0 * f_longitude));
}



/** MAP DRAWING ROUTINES **/


/**********************************************************
 * draw_grid()
 *
 * Draws a lat/lon grid on top of the view.
 **********************************************************/
void draw_grid(Widget w) {
    int place;
    char place_str[10];
    int grid_place;
    long xx, yy, xx1, yy1;

    /* Set the line width in the GC */
    (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineOnOffDash, CapButt,JoinMiter);
    (void)XSetForeground (XtDisplay (w), gc, colors[0x08]);

    //printf("scale_x: %lu\n", scale_x);

    if (scale_x < GRID_MORE)
        grid_place = 1;
    else
        grid_place = 10;

    if (long_lat_grid) {
        for (place = 180; place >= 0; place -= grid_place) {
            xastir_snprintf(place_str, sizeof(place_str), "%03d00.00W", place);
            /*printf("Place %s\n",place_str); */
            xx1 = xx = ((convert_lon_s2l (place_str) - x_long_offset) / scale_x);
            if (xx > 0 && xx < screen_width) {
                yy  = (convert_lat_s2l ("9000.00N") - y_lat_offset) / scale_y;
                yy1 = (convert_lat_s2l ("9000.00S") - y_lat_offset) / scale_y;
                if (yy < 0)
                    yy = 0;

                if (yy1 > screen_height)
                    yy1 = screen_height;

                (void)XDrawLine (XtDisplay (w), pixmap_final, gc, xx, yy, xx1, yy1);
            }
        }
        for (place = grid_place; place < 181; place += grid_place) {
            xastir_snprintf(place_str, sizeof(place_str), "%03d00.00E", place);
            /*printf("Place %s\n",place_str); */
            xx1 = xx = ((convert_lon_s2l (place_str) - x_long_offset) / scale_x);
            if (xx > 0 && xx < screen_width) {
                yy  = (convert_lat_s2l ("9000.00N") - y_lat_offset) / scale_y;
                yy1 = (convert_lat_s2l ("9000.00S") - y_lat_offset) / scale_y;
                if (yy < 0)
                    yy = 0;

                if (yy1 > screen_height)
                    yy1 = screen_height;

                (void)XDrawLine (XtDisplay (w), pixmap_final, gc, xx, yy, xx1, yy1);
            }
        }
        for (place = 90; place >= 0; place -= grid_place) {
            xastir_snprintf(place_str, sizeof(place_str), "%02d00.00N", place);
            /*printf("Place %s\n",place_str); */
            yy1 = yy = ((convert_lat_s2l (place_str) - y_lat_offset) / scale_y);

            if (yy > 0 && yy < screen_height) {
                xx  = (convert_lon_s2l ("18000.00W") - x_long_offset) / scale_x;
                xx1 = (convert_lon_s2l ("18000.00E") - x_long_offset) / scale_x;
                if (xx < 0)
                    xx = 0;

                if (xx1 > screen_width)
                    xx1 = screen_width;

                (void)XDrawLine (XtDisplay (w), pixmap_final, gc, xx, yy, xx1, yy1);
            }
        }
        for (place = grid_place; place < 91; place += grid_place) {
            xastir_snprintf(place_str, sizeof(place_str), "%02d00.00S", place);
            /*printf("Place %s\n",place_str); */
            yy1 = yy = ((convert_lat_s2l (place_str) - y_lat_offset) / scale_y);

            if (yy > 0 && yy < screen_height) {
                xx  = (convert_lon_s2l ("18000.00W") - x_long_offset) / scale_x;
                xx1 = (convert_lon_s2l ("18000.00E") - x_long_offset) / scale_x;
                if (xx < 0)
                    xx = 0;

                if (xx1 > screen_width)
                    xx1 = screen_width;

                (void)XDrawLine (XtDisplay (w), pixmap_final, gc, xx, yy, xx1, yy1);
            }
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
void map_plot (Widget w, long max_x, long max_y, long x_long_cord,long y_lat_cord,
    unsigned char color, long object_behavior, int destination_pixmap) {
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
            printf(" MAP Plot - max_x: %ld, max_y: %ld, x: %ld, y: %ld, color: %d, behavior: %lx, points: %d\n",
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
                        if (map_color_fill)
                            (void)XFillPolygon (XtDisplay (w), pixmap, gc, points, npoints, Complex,CoordModeOrigin);
                        break;

                    case DRAW_TO_PIXMAP_ALERTS:
                        // We must be drawing weather alert maps 'cuz this is the pixmap we use for it.
                        // This GC is used only for pixmap_alerts
                        (void)XSetForeground (XtDisplay (w), gc_tint, colors[(int)fill_color]);

                        // This is how we tint it instead of obscuring the whole map
// N7TAP
// I'm not sure what this is actually going to draw, now that we use draw_shapefile_map
// for weather alerts...  But, I don't want GXor for weather alerts anyway since it
// would change the colors of the alerts, losing that bit of information.
//                        (void)XSetFunction (XtDisplay (w), gc_tint, GXor);
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


                        // Here we wish to tint the existing map instead of obscuring it.  We'll use
                        // gc_tint here instead of gc.
                        (void)XFillPolygon (XtDisplay (w), pixmap_alerts, gc_tint, points, npoints, Complex,CoordModeOrigin);
                        break;

                    case DRAW_TO_PIXMAP_FINAL:
                        // We must be drawing symbols/tracks 'cuz this is the pixmap we use for it.
                        (void)XFillPolygon (XtDisplay (w), pixmap_final, gc, points, npoints, Complex,CoordModeOrigin);
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
                    // This GC is used only for pixmap_alerts
                    (void)XSetForeground (XtDisplay (w), gc_tint, colors[(int)last_color]);

                    // Here we wish to tint the existing map instead of obscuring it.  We'll use
                    // gc_tint here instead of gc.
                    (void)XDrawLines (XtDisplay (w), pixmap_alerts, gc_tint, points, npoints,CoordModeOrigin);
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
    char fname[128];

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
    //printf("FGD Directory: %s\n", fullpath);
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
    char fname[128];

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
        printf ("              Bottom     Top       Left     Right\n");

        printf ("View Edges:  %lu  %lu  %lu  %lu\n",
            view_max_y,
            view_min_y,
            view_min_x,
            view_max_x);

        printf (" Map Edges:  %lu  %lu  %lu  %lu\n",
            bottom_map_boundary,
            top_map_boundary,
            left_map_boundary,
            right_map_boundary);

        if ((left_map_boundary <= view_max_x) && (left_map_boundary >= view_min_x))
            printf ("Left map boundary inside view\n");

        if ((right_map_boundary <= view_max_x) && (right_map_boundary >= view_min_x))
            printf ("Right map boundary inside view\n");

        if ((top_map_boundary <= view_max_y) && (top_map_boundary >= view_min_y))
            printf ("Top map boundary inside view\n");

        if ((bottom_map_boundary <= view_max_y) && (bottom_map_boundary >= view_min_y))
            printf ("Bottom map boundary inside view\n");

        if ((view_max_x <= right_map_boundary) && (view_max_x >= left_map_boundary))
            printf ("Right view boundary inside map\n");

        if ((view_min_x <= right_map_boundary) && (view_min_x >= left_map_boundary))
            printf ("Left view boundary inside map\n");

        if ((view_max_y <= bottom_map_boundary) && (view_max_y >= top_map_boundary))
            printf ("Bottom view boundary inside map\n");

        if ((view_min_y <= bottom_map_boundary) && (view_min_y >= top_map_boundary))
            printf ("Top view boundary inside map\n");
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
        printf("map_inside_view: %d  view_inside_map: %d  parallel_edges: %d\n",
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
                         double f_right_map_boundary)
{
    unsigned long bottom_map_boundary,
                  top_map_boundary,
                  left_map_boundary,
                  right_map_boundary;

    (void)convert_to_xastir_coordinates ( &left_map_boundary,
                                    &top_map_boundary,
                                    (float)f_left_map_boundary,
                                    (float)f_top_map_boundary );

    (void)convert_to_xastir_coordinates ( &right_map_boundary,
                                    &bottom_map_boundary,
                                    (float)f_right_map_boundary,
                                    (float)f_bottom_map_boundary );


    return(map_visible( bottom_map_boundary,
                        top_map_boundary,
                        left_map_boundary,
                        right_map_boundary) );
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





//WE7U6
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
    char *fontname=
//        "-adobe-helvetica-medium-o-normal--24-240-75-75-p-130-iso8859-1";
//        "-adobe-helvetica-medium-o-normal--12-120-75-75-p-67-iso8859-1";
//        "-adobe-helvetica-medium-r-*-*-*-130-*-*-*-*-*-*";
//        "-*-times-bold-r-*-*-13-*-*-*-*-80-*-*";
//        "-*-helvetica-bold-r-*-*-14-*-*-*-*-80-*-*";
        "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";

    static XFontStruct *font=NULL;
//    XPoint *corner;
//    int i;
    float my_rotation = (float)((-rotation)-90);


    /* load font */
    if(!font) 
        font=(XFontStruct *)XLoadQueryFont (XtDisplay (w), fontname);


    // Code to determine the bounding box corner points for the rotated text
//    corner = XRotTextExtents(w,font,my_rotation,x,y,label_text,BLEFT);
//    for (i=0;i<5;i++) {
//        printf("%d,%d\t",corner[i].x,corner[i].y);
//    }
//    printf("\n");

    (void)XSetForeground (XtDisplay (w), gc, color);

    //printf("%0.1f\t%s\n",my_rotation,label_text);

    if (       ( (my_rotation < -90.0) && (my_rotation > -270.0) )
            || ( (my_rotation >  90.0) && (my_rotation <  270.0) ) ) {
        my_rotation = my_rotation + 180.0;
        (void)XRotDrawAlignedString(XtDisplay (w), font, my_rotation, pixmap, gc, x, y, label_text, BRIGHT);
    }
    else {
        (void)XRotDrawAlignedString(XtDisplay (w), font, my_rotation, pixmap, gc, x, y, label_text, BLEFT);
    }
}





#ifdef HAVE_SHAPELIB
/**********************************************************
 * draw_shapefile_map()
 *
 * The current implementation can draw only ESRI polygon
 * or PolyLine shapefiles.
 *
 * Need to modify this routine:  If alert is NULL, draw
 * every shape that fits the screen.  If non-NULL, draw only
 * the shape that matches the zone number.
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
                        int destination_pixmap) {

    DBFHandle       hDBF;
    SHPObject       *object;
    static XPoint   points[MAX_MAP_POINTS];
    char            file[2000];        /* Complete path/name of image file */
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
    int             road_flag = 0;
    int             water_flag = 0;
    int             weather_alert_flag = 0;
    int             lake_flag = 0;
    char            *filename;  // filename itself w/o directory
    char            search_param1[10];
    int             search_field1 = 0;
    char            search_param2[10];
    int             search_field2 = -1;
    int             found_shape = -1;
    int             start_record;
    int             end_record;
    int             ok_to_draw = 0;

    //printf("*** Alert color: %d ***\n",alert_color);

    // We don't draw the shapes if alert_color == -1
    if (alert_color != -1)
        ok_to_draw++;

    search_param1[0] = '\0';
    search_param2[0] = '\0';

    xastir_snprintf(file, sizeof(file), "%s/%s", dir, filenm);

    filename = filenm;
    i = strlen(filenm);
    while ( (filenm[i] != '/') && (i >= 0) )
        filename = &filenm[i--];
        //printf("draw_shapefile_map:filename:%s\ttitle:%s\n",filename,alert->title);    

    if (alert)
        weather_alert_flag++;

    // Open the .dbf file for reading.  This has the textual
    // data (attributes) associated with each shape.
    hDBF = DBFOpen( file, "rb" );
    if ( hDBF == NULL ) {
        if (debug_level & 16)
            printf("draw_shapefile_map: DBFOpen(%s,\"rb\") failed.\n", file );

        return;
    }

    if (debug_level & 16)
        printf ("\n---------------------------------------------\nInfo for %s\n",filenm);

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
        printf ("%d Columns,  %d Records in file\n", fieldcount, recordcount);

    panWidth = (int *) malloc( fieldcount * sizeof(int) );

    if (strncasecmp (filename, "lk", 2) == 0) {
        lake_flag++;
        water_flag++;

        if (debug_level & 16)
            printf("*** Found some lakes ***\n");
    }


    if (weather_alert_flag) {

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
                snprintf(search_param1,sizeof(search_param1),"%c%c",
                    alert->title[0],
                    alert->title[1]);
// Correct.
                snprintf(search_param2,sizeof(search_param2),"%c%c%c",
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
// Correct
                snprintf(search_param1,sizeof(search_param1),"%c%c%c",
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
// Correct
                snprintf(search_param1,sizeof(search_param1),"%c%c%c%c%c%c",
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
// Correct
                snprintf(search_param1,sizeof(search_param1),"%c%c%c%c%c%c",
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
// Correct
                snprintf(search_param1,sizeof(search_param1),"%c%c%c%c%c",
                    alert->title[0],
                    alert->title[1],
                    alert->title[4],
                    alert->title[5],
                   alert->title[6]);
                break;
        }

        //printf("Search_param1: %s,\t",search_param1);
        //printf("Search_param2: %s\n",search_param2);
    }

    for( i = 0; i < fieldcount; i++ ) {
        char            szTitle[12];

        switch ( DBFGetFieldInfo( hDBF, i, szTitle, &nWidth, &nDecimals )) {
              case FTString:
                strcpy (ftype, "string");;
                break;

              case FTInteger:
                strcpy (ftype, "integer");
                break;

              case FTDouble:
                strcpy (ftype, "float");
                break;

              case FTInvalid:
                strcpy (ftype, "invalid/unsupported");
                break;

              default:
                strcpy (ftype, "unknown");
                break;
        }

        if (debug_level & 16)
            printf ("%15.15s\t%15s  (%d,%d)\n",szTitle, ftype, nWidth, nDecimals);

        if (strncasecmp (szTitle, "SPEEDLIM", 8) == 0) {
            road_flag++;

            if (debug_level & 16)
                printf("*** Found some roads ***\n");
        }
        if (strncasecmp (szTitle, "US_RIVS_ID", 10) == 0)
        {

            water_flag++;

            if (debug_level & 16)
                printf("*** Found some water ***\n");
        }
    }

    // Search for specific record if we're doing alerts
    if (weather_alert_flag) {
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
                    string1 = (char *)DBFReadStringAttribute(hDBF,i,search_field1);
                    if (!strncasecmp(search_param1,string1,2)) {
                        //printf("Found state\n");
                        string2 = (char *)DBFReadStringAttribute(hDBF,i,search_field2);
                        ptr = string2;
                        ptr += 2;   // Skip past first two characters of FIPS code
                        if (!strncasecmp(search_param2,ptr,3)) {
//printf("Found it!  %s\tShape: %d\n",string1,i);
                            done++;
                            found_shape = i;
                        }
                    }
                    break;
                case 'w':   // County Warning Area File
                    string1 = (char *)DBFReadStringAttribute(hDBF,i,search_field1);
                    if (!strncasecmp(search_param1,string1,strlen(string1))) {
//printf("Found it!  %s\tShape: %d\n",string1,i);
                        done++;
                        found_shape = i;
                    }
                    break;
                case 'o':   // Offshore Marine Area File
                    string1 = (char *)DBFReadStringAttribute(hDBF,i,search_field1);
                    if (!strncasecmp(search_param1,string1,strlen(string1))) {
//printf("Found it!  %s\tShape: %d\n",string1,i);
                        done++;
                        found_shape = i;
                    }
                    break;
                case 'm':   // Marine Area File
                    string1 = (char *)DBFReadStringAttribute(hDBF,i,search_field1);
                    if (!strncasecmp(search_param1,string1,strlen(string1))) {
//printf("Found it!  %s\tShape: %d\n",string1,i);
                        done++;
                        found_shape = i;
                    }
                    break;
                case 'z':   // Zone File
                    string1 = (char *)DBFReadStringAttribute(hDBF,i,search_field1);
                    if (!strncasecmp(search_param1,string1,strlen(string1))) {
//printf("Found it!  %s\tShape: %d\n",string1,i);
                        done++;
                        found_shape = i;
                    }
                default:
                    break;
            }
        }
    }


    // Open the .shx/.shp files for reading.
    // These are the index and the vertice files.
    hSHP = SHPOpen( file, "rb" );
    if( hSHP == NULL ) {
        printf("draw_shapefile_map: SHPOpen(%s,\"rb\") failed.\n", file );
        DBFClose( hDBF );   // Clean up open file descriptors
        return;
    }

    SHPGetInfo( hSHP, &nEntities, &nShapeType, adfBndsMin, adfBndsMax );

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
            strcpy(sType,"MultiPoint");
            break;

        default:
            DBFClose( hDBF );   // Clean up open file descriptors
            SHPClose( hSHP );
            return; // Unknown type.  Don't know how to process it.
            break;
    }

    if (debug_level & 16)
        printf ("%s(%d), %d Records in file\n",sType,nShapeType,nEntities);

    if (debug_level & 16)
        printf( "File Bounds: (%15.10g,%15.10g)\n\t(%15.10g,%15.10g)\n",
            adfBndsMin[0], adfBndsMin[1], adfBndsMax[0], adfBndsMax[1] );


    if (! map_visible_lat_lon(  adfBndsMin[1],       // Bottom
                                adfBndsMax[1],       // Top
                                adfBndsMin[0],       // Left
                                adfBndsMax[0]) ) {   // Right
        if (debug_level & 16)
            printf("No shapes within viewport.  Skipping file...\n");

        DBFClose( hDBF );   // Clean up open file descriptors
        SHPClose( hSHP );
        return;     // The file contains no shapes in our viewport
    }

    (void)XSetForeground (XtDisplay (w), gc, colors[(int)0x00]);
 
    if (weather_alert_flag) {
        char xbm_path[500];
        int _w, _h, _xh, _yh;
        // This GC is used only for pixmap_alerts
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
        XReadBitmapFile(XtDisplay(w), DefaultRootWindow(XtDisplay(w)),
                        xbm_path, &_w, &_h, &pixmap_wx_stipple, &_xh, &_yh);
        (void)XSetStipple(XtDisplay(w), gc_tint, pixmap_wx_stipple);
    } else {
        if (water_flag)
            (void)XSetForeground (XtDisplay (w), gc, colors[(int)0x09]);

//        if (road_flag)
        else
            (void)XSetForeground (XtDisplay (w), gc, colors[(int)0x08]);
    }


    // Now that we have the file open, we can read out the structures.
    // We can handle PolyLine and Polygon shapefiles at the moment.
    // Polygons:  If you read the spec closely you'll see that some of
    // the rings (parts) can be holes, depending on which direction the
    // vertices run.  Polygon holes have vertices running in the
    // counterclockwise direction.  Filled polygons have vertices running
    // in the clockwise direction.  I'm ignoring that for now, drawing ALL
    // polygon shapes found.


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

    for (structure = start_record; structure < end_record; structure++) {

        object = SHPReadObject( hSHP, structure );  // Note that each structure can have multiple rings

        if (object == NULL)
            continue;   // Skip this iteration, go on to the next

        // Here we check the bounding box against our current viewport.
        // If we can't see it, don't draw it.  bottom/top/left/right
        if ( map_visible_lat_lon( object->dfYMin,       // Bottom
                                  object->dfYMax,       // Top
                                  object->dfXMin,       // Left
                                  object->dfXMax) ) {   // Right
            const char *temp;
            int jj;

            //printf("Shape %d is visible, drawing it\t", structure);
            //printf( "Parts in shape: %d\n", object->nParts );    // Number of parts in this structure

            if (alert)
                alert->flags[0] = 'Y';


            if (debug_level & 16) {
                // Print the field contents
                for (jj = 0; jj < fieldcount; jj++) {
                    temp = DBFReadStringAttribute( hDBF, structure, jj );
                    if (temp != NULL) {
                        printf("%s, ", temp);
                    }
                }
                printf("\n");
            }

            switch ( nShapeType ) {
                case SHPT_POINT:
                        // Not implemented.
                    break;

                case SHPT_ARC:
                    temp = "";

                    if (road_flag) {
                        // For roads, we need to use SIGN1 if it exists, else use DESCRIP if it exists.
                        temp = DBFReadStringAttribute( hDBF, structure, 9 );    // SIGN1

                        if ( (temp == NULL) || (strlen(temp) == 0) ) {
                            temp = DBFReadStringAttribute( hDBF, structure, 12 );    // DESCRIP
                            
                        }
                    } else if (water_flag) {
                        temp = DBFReadStringAttribute( hDBF, structure, 13 );   // PNAME (rivers)
                    }

                    if ( (temp != NULL) && (strlen(temp) != 0) && (map_labels) ) {
                        ok = 1;

                        // Convert to Xastir coordinates
                        my_long = (unsigned long)(64800000l + (360000.0 * object->padfX[0] ) );
                        my_lat  = (unsigned long)(32400000l + (360000.0 * (-object->padfY[0]) ) );
                        //printf("%ld %ld\n", my_long, my_lat);

                        // Convert to screen coordinates
                        x = (long)( (my_long - x_long_offset) / scale_x);
                        y = (long)( (my_lat - y_lat_offset) / scale_y);

                        if (x >  16000) ok = 0;     // Skip this point
                        if (x < -16000) ok = 0;     // Skip this point
                        if (y >  16000) ok = 0;     // Skip this point
                        if (y < -16000) ok = 0;     // Skip this point

                        if (ok == 1 && ok_to_draw) {
                            (void)draw_label_text ( w, x, y, strlen(temp), colors[0x08], (char *)temp);
                        }
                    }

                    index = 0;
                    // Read the vertices for each line
                    for (ring = 0; ring < object->nVertices; ring++ ) {
                        ok = 1;

                        //printf("\t%d:%g %g\t", ring, object->padfX[ring], object->padfY[ring] );
                        // Convert to Xastir coordinates
                        my_long = (unsigned long)(64800000l + (360000.0 * object->padfX[ring] ) );
                        my_lat  = (unsigned long)(32400000l + (360000.0 * (-object->padfY[ring]) ) );
                        //printf("%ld %ld\n", my_long, my_lat);

                        // Convert to screen coordinates
                        x = (long)( (my_long - x_long_offset) / scale_x);
                        y = (long)( (my_lat - y_lat_offset) / scale_y);

                        if (x >  16000) ok = 0;     // Skip this point
                        if (x < -16000) ok = 0;     // Skip this point
                        if (y >  16000) ok = 0;     // Skip this point
                        if (y < -16000) ok = 0;     // Skip this point

                        if (ok == 1) {
                            points[index].x = (short)x;
                            points[index].y = (short)y;
                            //printf("%d %d\t", points[index].x, points[index].y);
                            index++;
                        }
                    }

                    if (road_flag) {
                        int lanes;

                        lanes = DBFReadIntegerAttribute( hDBF, structure, 6 );
                        if (lanes != (int)NULL) {
                            (void)XSetLineAttributes (XtDisplay (w), gc, lanes, LineSolid, CapButt,JoinMiter);
                        }
                    }
                    else if (water_flag) {
                        (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineSolid, CapButt,JoinMiter);
                    }
                    else {  // Set default line width
                        (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineSolid, CapButt,JoinMiter);
                    }

                    if (ok_to_draw) {
                        int temp;

                        //printf("Index = %d\n", index);
                        //(void)XSetForeground (XtDisplay (w), gc, colors[(int)0x4e]);
                        temp = XDrawLines (XtDisplay(w), pixmap, gc, points, index, CoordModeOrigin);
                        if (temp != 0) {
                            //printf("*** BAD XDrawLines return value: %d ***\n", temp);
                        }
                    }
                    break;

                case SHPT_POLYGON:

                   // Read the vertices for each ring
                    for (ring = 0; ring < object->nParts; ring++ ) {
                        int endpoint;

                        //printf("Ring: %d\t\t", ring);

                        if ( (ring+1) < object->nParts)
                            endpoint = object->panPartStart[ring+1];
                            //else endpoint = object->nVertices;
                        else
                            endpoint = object->panPartStart[0] + object->nVertices;

                        //printf("Endpoint %d\n", endpoint);
                        //printf("Vertices: %d\n", endpoint - object->panPartStart[ring]);

                        i = 0;  // Number of points to draw
                        for (index = object->panPartStart[ring]; index < endpoint; ) {
                            ok = 1;

                            //printf("\t%d:%g %g\t", index, object->padfX[index], object->padfY[index] );

                            // Get vertice and convert to Xastir coordinates
                            my_long = (unsigned long)(64800000l + (360000.0 * object->padfX[index] ) );
                            my_lat  = (unsigned long)(32400000l + (360000.0 * (-object->padfY[index]) ) );

                            //printf("%lu %lu\t", my_long, my_lat);

                            // Convert to screen coordinates

                            x = (long)( ((long)(my_long - x_long_offset)) / scale_x);
                            y = (long)( ((long)(my_lat - y_lat_offset)) / scale_y);

                            //printf("%ld %ld\t\t", x, y);

                            // Here we check for really wacko points that will cause problems
                            // with the X drawing routines, and fix them.
                            if (x >  15000l) x =  15000l;
                            if (x < -15000l) x = -15000l;
                            if (y >  15000l) y =  15000l;
                            if (y < -15000l) y = -15000l;

                            if ((x > 16000l) || (x < -16000l) || (y > 16000l) || (y < -16000l)) {
                                ok = 0;
                                //printf("Out of range for XFillPolygon()... Not drawing ring %d\n", ring);
                                //printf("x: %ld\t\ty: %ld\n", x, y);
                                index++;
                                break;
                            }

                            points[i].x = (short)x;
                            points[i].y = (short)y;


                            //printf("%d %d\t", points[i].x, points[i].y);

                            if (       (x < -16383)
                                    || (x > 16383)
                                    || (y < -16383)
                                    || (y > 16383) ) {
                                //printf("Out of Range\n");
                            } else {
                                //printf("\n");
                            }

                            index++;
                            i++;    // Number of points to draw
                        }
                        if (i >= 3 && ok_to_draw) {   // We have a polygon to draw
                            //(void)XSetForeground(XtDisplay (w), gc, colors[(int)0x64]);
                            if (map_color_fill || water_flag || weather_alert_flag) {
                                if (weather_alert_flag) {
                                    (void)XSetFillStyle(XtDisplay(w), gc_tint, FillStippled);
                                    (void)XFillPolygon(XtDisplay(w), pixmap_alerts, gc_tint, points, i, Complex, CoordModeOrigin);
                                    (void)XSetFillStyle(XtDisplay(w), gc_tint, FillSolid);
                                    (void)XDrawLines(XtDisplay(w), pixmap_alerts, gc_tint, points, i, CoordModeOrigin);
                                }
                                else
                                    (void)XFillPolygon (XtDisplay (w), pixmap, gc, points, i, Complex, CoordModeOrigin);
                            } else {
                                int temp;

                                (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineSolid, CapButt,JoinMiter);
                                temp = XDrawLines(XtDisplay(w), pixmap, gc, points, i, CoordModeOrigin);
                                if (temp != 0) {
                                    //printf("*** BAD XDrawLines return value: %d ***\n", temp);
                                }
                            }
                        }

                    }
                    temp = "";

                    if (lake_flag) {
                        temp = DBFReadStringAttribute( hDBF, structure, 0 );    // NAME (lakes)
                    }

                    if ( (temp != NULL) && (strlen(temp) != 0) && (map_labels) ) {
                        ok = 1;

                        // Convert to Xastir coordinates
                        my_long = (unsigned long)(64800000l + (360000.0 * object->padfX[0] ) );
                        my_lat  = (unsigned long)(32400000l + (360000.0 * (-object->padfY[0]) ) );
                        //printf("%ld %ld\n", my_long, my_lat);

                        // Convert to screen coordinates
                        x = (long)( (my_long - x_long_offset) / scale_x);
                        y = (long)( (my_lat - y_lat_offset) / scale_y);

                        if (x >  16000) ok = 0;     // Skip this point
                        if (x < -16000) ok = 0;     // Skip this point
                        if (y >  16000) ok = 0;     // Skip this point
                        if (y < -16000) ok = 0;     // Skip this point

                        if (ok == 1 && ok_to_draw) {
                            (void)draw_label_text ( w, x, y, strlen(temp), colors[0x08], (char *)temp);
                        }
                    }
                    break;

                case SHPT_MULTIPOINT:
                        // Not implemented.
                    break;

                default:
                        // Not implemented.
                    break;
            }
        }
        else {  // Shape not currently visible
            if (alert)
                alert->flags[0] = 'N';
        }
        SHPDestroyObject( object ); // Done with this structure
    }
    DBFClose( hDBF );
    SHPClose( hSHP );
//    XmUpdateDisplay (XtParent (da));
}
#endif  // HAVE_SHAPELIB





void Print_properties_destroy_shell(/*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);

begin_critical_section(&print_properties_dialog_lock, "maps.c:Print_properties_destroy_shell" );

    XtDestroyWidget(shell);
    print_properties_dialog = (Widget)NULL;

end_critical_section(&print_properties_dialog_lock, "maps.c:Print_properties_destroy_shell" );

}





//WE7U4
// Print_window:  Prints the drawing area to a Postscript file.
//
void Print_window( Widget widget, XtPointer clientData, XtPointer callData ) {

#ifdef NO_GRAPHICS
    printf("XPM or ImageMagick support not compiled into Xastir!\n");
#else   // NO_GRAPHICS

   char xpm_filename[50];
    char ps_filename[50];
    char mono[50] = "";
    char invert[50] = "";
    char rotate[50] = "";
    char scale[50] = "";
    char density[50] = "";
    char command[300];
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
        printf( "Creating %s\n", xpm_filename );

    xastir_snprintf(temp, sizeof(temp), langcode("PRINT0012") );
    statusline(temp,1);       // Dumping image to file...

    if ( !XpmWriteFileFromPixmap(XtDisplay(appshell),  // Display *display
            xpm_filename,                               // char *filename
            pixmap_final,                               // Pixmap pixmap
            (Pixmap)NULL,                               // Pixmap shapemask
            NULL ) == XpmSuccess ) {                    // XpmAttributes *attributes
        printf ( "ERROR writing %s\n", xpm_filename );
    }
    else {          // We now have the xpm file created on disk

        chmod( xpm_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

        if ( debug_level & 512 )
            printf( "Convert %s ==> %s\n", xpm_filename, ps_filename );


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
            printf("Width: %ld\tHeight: %ld\n", screen_width, screen_height);


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
                    printf("Rotating\n");
            } else {
                rotate[0] = '\0';   // Empty string
                if (debug_level & 512)
                    printf("Not Rotating\n");
            }
        } else {
            rotate[0] = '\0';   // Empty string
            if (debug_level & 512)
                printf("Not Rotating\n");
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

        xastir_snprintf(command, sizeof(command), "convert -filter Point %s%s%s%s%s %s %s",
                mono, invert, rotate, scale, density, xpm_filename, ps_filename );
        if ( debug_level & 512 )
            printf( "%s\n", command );

        if ( system( command ) != 0 ) {
            printf("\n\nPrint: Couldn't convert from XPM to PS!\n\n\n");
            return;
        }

        chmod( ps_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

        // Delete temporary xpm file
        if ( !(debug_level & 512) )
            unlink( xpm_filename );

        if ( debug_level & 512 )
            printf("Printing postscript file %s\n", ps_filename);

//WE7U4
// Note: This needs to be changed to "lp" for Solaris.
// Also need to have a field to configure the printer name.  One
// fill-in field could do both.
//
// Since we could be running SUID root, we don't want to be
// calling "system" anyway.  Several problems with it.
/*
        xastir_snprintf(command, sizeof(command), "lpr -Plp %s", ps_filename );
        if ( debug_level & 512 )
            printf("%s\n", command);

        if ( system( command ) != 0 ) {
            printf("\n\nPrint: Couldn't send to the printer!\n\n\n");
            return;
        }
*/


        // Bring up the "gv" postscript viewer
        xastir_snprintf(command, sizeof(command), "gv %s-scale -2 -media Letter %s &",
                format, ps_filename );

        if ( debug_level & 512 )
            printf("%s\n", command);

        if ( system( command ) != 0 ) {
            printf("\n\nPrint: Couldn't bring up the gv viewer!\n\n\n");
            return;
        }
/*
        if ( !(debug_level & 512) )
            unlink( ps_filename );
*/

        if ( debug_level & 512 )
            printf("  Done printing.\n");
    }

    xastir_snprintf(temp, sizeof(temp), langcode("PRINT0014") );
    statusline(temp,1);       // Finished creating print file.

    //popup_message( langcode("PRINT0015"), langcode("PRINT0014") );

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





//WE7U4
// Print_properties:  Prints the drawing area to a PostScript file
//
// Perhaps later:
// 1) Select an area on the screen to print
// 2) -label
//
void Print_properties( Widget w, XtPointer clientData, XtPointer callData ) {
    static Widget pane, form, button_preview, button_ok, button_cancel,
            sep, paper_size, paper_size_data, auto_scale, scale,
            scale_data, blank_background, monochrome, invert, res_label1,
            res_label2, res_x, res_y;
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
                            XmNfractionBase, 3,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);


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


        auto_rotate  = XtVaCreateManagedWidget(langcode("PRINT0003"),xmToggleButtonWidgetClass,form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, paper_size_data,
                                      XmNtopOffset, 5,
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
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, paper_size_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, auto_rotate,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
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


        blank_background = XtVaCreateManagedWidget(langcode("PRINT0007"),xmToggleButtonWidgetClass,form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, scale_data,
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


        monochrome = XtVaCreateManagedWidget(langcode("PRINT0008"),xmToggleButtonWidgetClass,form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, blank_background,
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


        sep = XtVaCreateManagedWidget("Print_properties sep", xmSeparatorGadgetClass,form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, res_y,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNbackground, colors[0xff],
                                      NULL);


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


        button_ok = XtVaCreateManagedWidget(langcode("PRINT0011"),xmPushButtonGadgetClass, form,
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
                                      XmNleftPosition, 2,
                                      XmNleftOffset, 3,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 3,
                                      XmNrightOffset, 5,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);


        XtAddCallback(button_preview, XmNactivateCallback, Print_window, "1" );
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
 

        XmTextFieldSetString(paper_size_data,print_paper_size);


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





//WE7U4
// Create png image (for use in web browsers??).  Requires that "convert"
// from the ImageMagick package be installed on the system.
//
void Snapshot(void) {

#ifdef NO_GRAPHICS
    // Time to take another snapshot?
    if (sec_now() < (last_snapshot + 300) ) // New snapshot every five minutes
        return;

    last_snapshot = sec_now(); // Set up timer for next time
    printf("XPM or ImageMagick support not compiled into Xastir!\n");
    return;
#else // NO_GRAPHICS
 
    char xpm_filename[50];
    char png_filename[50];
    char command[200];
    uid_t user_id;
    struct passwd *user_info;
    char username[20];


    // Get user info 
    user_id=getuid();
    user_info=getpwuid(user_id);
    // Get my login name
    strcpy(username,user_info->pw_name);

    xastir_snprintf(xpm_filename, sizeof(xpm_filename), "/var/tmp/xastir_%s_snap.xpm",
            username);
    xastir_snprintf(png_filename, sizeof(png_filename), "/var/tmp/xastir_%s_snap.png",
            username);

    // Time to take another snapshot?
    if (sec_now() < (last_snapshot + 300) ) // New snapshot every five minutes
        return;

    last_snapshot = sec_now(); // Set up timer for next time

    if ( debug_level & 512 )
        printf( "Creating %s\n", xpm_filename );

    if ( !XpmWriteFileFromPixmap( XtDisplay(appshell),    // Display *display
            xpm_filename,                               // char *filename
            pixmap_final,                               // Pixmap pixmap
            (Pixmap)NULL,                               // Pixmap shapemask
            NULL ) == XpmSuccess ) {                    // XpmAttributes *attributes
        printf ( "ERROR writing %s\n", xpm_filename );
    } else {  // We now have the xpm file created on disk
        chmod( xpm_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

        if ( debug_level & 512 )
            printf( "Convert %s ==> %s\n", xpm_filename, png_filename );

        // Convert it to a png file.  This depends on having the
        // ImageMagick command "convert" installed.
        xastir_snprintf(command, sizeof(command), "convert -quality 100 %s %s",
                xpm_filename, png_filename );
        system( command );

        chmod( png_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

        // Delete temporary xpm file
        unlink( xpm_filename );

        if ( debug_level & 512 )
            printf("  Done creating png.\n");
    }

#endif // NO_GRAPHICS
}





//WE7U5
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
//
void draw_gnis_map (Widget w, char *dir, char *filenm)
{
    char file[2000];                // Complete path/name of GNIS file
    FILE *f;                        // Filehandle of GNIS file
    char line[400];                 // One line of text from file
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


    // Skip the file if map labels are disabled
    if (!map_labels)
        return;

    // Screen view
    min_lat = y_lat_offset + (long)(screen_height * scale_y);
    max_lat = y_lat_offset;
    min_lon = x_long_offset;
    max_lon = x_long_offset + (long)(screen_width  * scale_x);

    xastir_snprintf(file, sizeof(file), "%s/%s", dir, filenm);

    // Attempt to open the file
    f = fopen (file, "r");
    if (f != NULL) {
        while (!feof (f)) {     // Loop through entire file
            if ( get_line (f, line, 399) ) {  // Snag one line of data
                if (strlen(line) > 0) {

//NOTE:  How do we handle running off the end of "line" while using "index"?

                    // Find State feature resides in
                    i = index(line,',');    // Find ',' after state
                    i[0] = '\0';
                    strncpy(state,line+1,49);
                    state[strlen(state)-1] = '\0';

//NOTE:  It'd be nice to take the part after the comma and put it before the rest
// of the text someday, i.e. "Cassidy, Lake".
                    // Find Name
                    j = index(i+1, ',');    // Find ',' after Name.  Note that there may be commas in the name.
                    while (j[1] != '\"') {
                        k = j;
                        j = index(k+1, ',');
                    }
                    j[0] = '\0';
                    strncpy(name,i+2,199);
                    name[strlen(name)-1] = '\0';

                    // Find Type
                    i = index(j+1, ',');
                    i[0] = '\0';
                    strncpy(type,j+2,99);
                    type[strlen(type)-1] = '\0';

                    // Find County          // Can there be commas in the county name?
                    j = index(i+1, ',');
                    j[0] = '\0';
                    strncpy(county,i+2,99);
                    county[strlen(county)-1] = '\0';

                    // Find ?
                    i = index(j+1, ',');
                    i[0] = '\0';

                    // Find ?
                    j = index(i+1, ',');
                    j[0] = '\0';

                    // Find latitude (DDMMSSN)
                    i = index(j+1, ',');
                    i[0] = '\0';
                    strncpy(latitude,j+2,14);
                    latitude[strlen(latitude)-1] = '\0';

                    // Find longitude (DDDMMSSW)
                    j = index(i+1, ',');
                    j[0] = '\0';
                    strncpy(longitude,i+2,14);
                    longitude[strlen(longitude)-1] = '\0';

                    // Find another latitude
                    i = index(j+1, ',');
                    i[0] = '\0';

                    // Find another longitude
                    j = index(i+1, ',');
                    j[0] = '\0';

                    // Find ?
                    i = index(j+1, ',');
                    i[0] = '\0';

                    // Find ?
                    j = index(i+1, ',');
                    j[0] = '\0';

                    // Find ?
                    i = index(j+1, ',');
                    i[0] = '\0';

                    // Find ?
                    j = index(i+1, ',');
                    j[0] = '\0';

                    // Find altitude
                    i = index(j+1, ',');
                    i[0] = '\0';

                    // Find population
                    j = index(i+1, ',');
                    j[0] = '\0';
                    strncpy(population,i+2,14);
                    population[strlen(population)-1] = '\0';
 
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

                    // Now check whether this lat/lon is within our viewport.  If it
                    // is, draw a text label at that location.
                    if (coord_lon >= min_lon && coord_lon <= max_lon
                            && coord_lat <= min_lat && coord_lat >= max_lat) {

                        if (debug_level & 16) {
                            printf("%s\t%s\t%s\t%s\t%s\t%s\t\t",
                                    state, name, type, county, latitude, longitude);
                            printf("%s %s %s %s\t%s %s %s %s\t\t",
                                    lat_dd, lat_mm, lat_ss, lat_dir, long_dd, long_mm, long_ss, long_dir);
                            printf("%s\t%s\n", lat_str, long_str);
                        }

                         // Convert to screen coordinates
                        x = (long)( (coord_lon - x_long_offset) / scale_x);
                        y = (long)( (coord_lat - y_lat_offset) / scale_y);

                        ok = 1;
                        if (x >  16000) ok = 0;     // Skip this point
                        if (x < -16000) ok = 0;     // Skip this point
                        if (y >  16000) ok = 0;     // Skip this point
                        if (y < -16000) ok = 0;     // Skip this point


                        if (strcasecmp(type,"airport") == 0) {
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
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"channel") == 0) {
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"church") == 0) {
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
                            if (scale_y > 50)
                                ok = 0;
                        }
                        else if (strcasecmp(type,"ppl") == 0) {
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
                                    //printf("Name: %s\tPopulation: %s\n",name,population);
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
                            printf("Something unusual found, Type:%s\tState:%s\tCounty:%s\tName:%s\n",
                                type,state,county,name);
                        }


                        if (ok == 1) {  // If ok to draw it
                            // colors[0x08] = Black
                            (void)draw_label_text ( w, x, y, strlen(name), colors[0x08], (char *)name);
                        }

                    }
                    else {
                        //printf("Not in viewport.  Coordinates: %ld %ld\n",coord_lat,coord_lon);
                        //printf("Min/Max Lat: %ld %ld\n",min_lat,max_lat);
                        //printf("Min/Max Lon: %ld %ld\n",min_lon,max_lon);
                    }
                }
            }
        }
        (void)fclose (f);
    }
    else {
        printf("Couldn't open file: %s\n", file);
        return;
    }
}





//WE7U5
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
int  locate_place( Widget w, char *name_in, char *state_in, char *county_in,
        char *quad_in, char *type_in, char *filename_in, int follow_case, int get_match ) {
    char file[2000];                // Complete path/name of GNIS file
    FILE *f;                        // Filehandle of GNIS file
    char line[400];                 // One line of text from file
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
        printf("File: %s\n",file);


    strcpy(name_in2,name_in);
    strcpy(state_in2,state_in);
    strcpy(county_in2,county_in);
    strcpy(quad_in2,quad_in);
    strcpy(type_in2,type_in);


    // Convert State/Province to upper-case always (they're
    // always upper-case in the GNIS files from USGS.
    to_upper(state_in2);


    if (debug_level & 16)
        printf("Name:%s\tState:%s\tCounty:%s\tQuad:%s\tType:%s\n",
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
            if ( get_line (f, line, 399) ) {  // Snag one line of data
                if (strlen(line) > 0) {


//NOTE:  How do we handle running off the end of "line" while using "index"?
// Short lines here can cause segfaults.


                    // Find State feature resides in
                    i = index(line,',');    // Find ',' after state
                    i[0] = '\0';
                    strncpy(state,line+1,49);
                    state[strlen(state)-1] = '\0';

//NOTE:  It'd be nice to take the part after the comma and put it before the rest
// of the text someday, i.e. "Cassidy, Lake".
                    // Find Name
                    j = index(i+1, ',');    // Find ',' after Name.  Note that there may be commas in the name.
                    while (j[1] != '\"') {
                        k = j;
                        j = index(k+1, ',');
                    }
                    j[0] = '\0';
                    strncpy(name,i+2,199);
                    name[strlen(name)-1] = '\0';

                    // Find Type
                    i = index(j+1, ',');
                    i[0] = '\0';
                    strncpy(type,j+2,99);
                    type[strlen(type)-1] = '\0';

                    // Find County          // Can there be commas in the county name?
                    j = index(i+1, ',');
                    j[0] = '\0';
                    strncpy(county,i+2,99);
                    county[strlen(county)-1] = '\0';

                    // Find ?
                    i = index(j+1, ',');
                    i[0] = '\0';

                    // Find ?
                    j = index(i+1, ',');
                    j[0] = '\0';

                    // Find latitude (DDMMSSN)
                    i = index(j+1, ',');
                    i[0] = '\0';
                    strncpy(latitude,j+2,14);
                    latitude[strlen(latitude)-1] = '\0';

                    // Find longitude (DDDMMSSW)
                    j = index(i+1, ',');
                    j[0] = '\0';
                    strncpy(longitude,i+2,14);
                    longitude[strlen(longitude)-1] = '\0';

                    // Find another latitude
                    i = index(j+1, ',');
                    i[0] = '\0';

                    // Find another longitude
                    j = index(i+1, ',');
                    j[0] = '\0';

                    // Find ?
                    i = index(j+1, ',');
                    i[0] = '\0';

                    // Find ?
                    j = index(i+1, ',');
                    j[0] = '\0';

                    // Find ?
                    i = index(j+1, ',');
                    i[0] = '\0';

                    // Find ?
                    j = index(i+1, ',');
                    j[0] = '\0';

                    // Find altitude
                    i = index(j+1, ',');
                    i[0] = '\0';

                    // Find population
                    j = index(i+1, ',');
                    j[0] = '\0';
                    strncpy(population,i+2,14);
                    population[strlen(population)-1] = '\0';
 
                    // Find quad name (last field)
                    i = index(j+1, '"');
                    i[0] = '\0';
                    strncpy(quad,j+2,14);
                    quad[strlen(quad)] = '\0';


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
                            printf("Match: %s,%s,%s,%s\n",name,state,county,type);

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

void draw_geo_image_map (Widget w, char *dir, char *filenm) {

#ifdef NO_GRAPHICS
    printf("No XPM or ImageMagick support compiled into Xastir!\n");
    return;
#else   // NO_GRAPHICS

    uid_t user_id;
    struct passwd *user_info;
    char username[20];
    char file[2000];                // Complete path/name of image file
    FILE *f;                        // Filehandle of image file
    char line[400];                 // One line from GEO file
    char fileimg[400];              // Ascii name of image file, read from GEO file
    char remote_callsign[400];      // Ascii callsign, read from GEO file, used for findu lookups only
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
    char local_filename[150];
    ExceptionInfo exception;
    Image *image;
    ImageInfo *image_info;
    PixelPacket pixel_pack;
    PixelPacket temp_pack;
    IndexPacket *index_pack;
    int l;
    XColor my_colors[256];
/*    Colormap cmap;  KD6ZWR - now set in main()*/
    int DirectClass = 0;
    char tempfile[2000];
    double left, right, top, bottom, map_width, map_height;
    double lat_center  = 0;
    double long_center = 0;
    XVisualInfo *visual_list;
    XVisualInfo visual_template;
    int visuals_matched;
    // Terraserver variables
    double top_n=0, left_e=0, bottom_n=0, right_e=0, map_top_n=0, map_left_e=0;
    int z, url_n=0, url_e=0, t_zoom=16, t_scale=12800;
    char zstr[8];
#else   // HAVE_IMAGEMAGICK
    XImage *xi;                 // Temp XImage used for reading in current image
#endif // HAVE_IMAGEMAGICK

    int tiger_flag = 0;
    int findu_flag = 0;
    int terraserver_flag = 0;
    char map_it[300];
    int geo_image_width;        // Image width  from GEO file
    int geo_image_height;       // Image height from GEO file
    char geo_datum[8+1];        // WGS-84 etc.
    char geo_projection[8+1];   // TM, UTM, GK, LATLON etc.
    int map_proj;


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
            (void)get_line (f, line, 399);
            if (strncasecmp (line, "FILENAME", 8) == 0)
                (void)sscanf (line + 9, "%s", fileimg);

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

            if (strncasecmp (line, "TIGERMAP", 8) == 0)
                tiger_flag = 1;
            else if (strncasecmp (line, "FINDU", 5) == 0)
                findu_flag = 1;
            else if (strncasecmp (line, "TERRASERVER", 11) == 0)
                terraserver_flag = 1;

            if (strncasecmp (line, "CALL", 4) == 0)
                (void)sscanf (line + 5, "%s", remote_callsign);
        }
        (void)fclose (f);
    }
    else {
        printf("Couldn't open file: %s\n", file);
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
//        printf("Map Datum: %s\n",geo_datum);   // not used now...

    if (geo_projection[0] == '\0')
        strcpy(geo_projection,"LatLon");        // default
    //printf("Map Projection: %s\n",geo_projection);    
    (void)to_upper(geo_projection);
    if (strcmp(geo_projection,"TM") == 0)
        map_proj = 1;           // Transverse Mercator
    else 
        map_proj = 0;           // Lat/Lon, default
    
//WE7U
#ifdef HAVE_IMAGEMAGICK
    if (tiger_flag || findu_flag) {  // Must generate our own calibration data for some maps

        // Tiepoint for upper left screen corner
        //
        tp[0].img_x = 0;                // Pixel Coordinates
        tp[0].img_y = 0;                // Pixel Coordinates
        tp[0].x_long = x_long_offset;   // Xastir Coordinates
        tp[0].y_lat  = y_lat_offset;    // Xastir Coordinates
        
        // Tiepoint for lower right screen corner
        //
        tp[1].img_x =  screen_width - 1; // Pixel Coordinates
        //
        // Here's one way to fix the map scaling problem, but it distorts the pixels
        // making the map ugly and making labels difficult to read.
        tp[1].img_y = screen_height - 1; // Pixel Coordinates (Original)
//        tp[1].img_y = screen_height * 1.5; // Pixel Coordinates (Fix)
        //
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

        if (debug_level >= 2) {
          printf("left side is %f\n", left);
          printf("right side is %f\n", right);
          printf("top  is %f\n", top);
          printf("bottom is %f\n", bottom);
          printf("screen width is %li\n", screen_width);
          printf("screen height is %li\n", screen_height);
          printf("map width is %f\n", map_width);
          printf("map height is %f\n", map_height);
        }
 
        long_center = (left + right)/2.0l;
        lat_center  = (top + bottom)/2.0l;

/*
        sprintf(fileimg,"\'http://tiger.census.gov/cgi-bin/mapgen?lat=%f\046lon=%f\046wid=%f\046ht=%f\046iwd=%li\046iht=%li\'",\
            lat_center, long_center, map_width, map_height, screen_width, screen_height);
        sprintf(fileimg,"\'http://tiger.census.gov/cgi-bin/mapgen?lat=%f\046lon=%f\046wid=%f\046ht=%f\046iwd=%i\046iht=%i\'",\
            lat_center, long_center, map_width, map_height, tp[1].img_x + 1, tp[1].img_x + 1);
*/

        if (tiger_flag) {
        if (debug_level & 512)   // Draw some calibration dots
                xastir_snprintf(fileimg, sizeof(fileimg), "\'http://tiger.census.gov/cgi-bin/mapgen?lat=%f\046lon=%f\046wid=%f\046ht=%f\046iwd=%i\046iht=%imark=-122,48;-122,47;-121,48;-121,47;-123,47;-123,48;-123,47.5;-122,47.5;-121,47.5\'",\
                        lat_center, long_center, map_width, map_height, tp[1].img_x + 1, tp[1].img_y + 1);
            else {
                // N0VH turned the grids and other data on for the census tiger maps
                xastir_snprintf(fileimg, sizeof(fileimg), "\'http://tiger.census.gov/cgi-bin/mapper/map.gif?on=GRID&on=places&&on=interstate&on=states&on=ushwy&lat=%f\046lon=%f\046wid=%f\046ht=%f\046iwd=%i\046iht=%i\'",\
                        lat_center, long_center, map_width, map_height, tp[1].img_x + 1, tp[1].img_y + 1);
            }
        } else if (findu_flag)   // Set up the URL for 2 weeks worth of raw data.
            xastir_snprintf(fileimg, sizeof(fileimg),
                "\'http://64.34.101.121/cgi-bin/rawposit.cgi?call=%s&start=336&length=336\'",
                remote_callsign);

        if (debug_level & 512) {
          printf("left side is %f\n", left);
          printf("right side is %f\n", right);
          printf("top  is %f\n", top);
          printf("bottom is %f\n", bottom);
          printf("lat center is %f\n", lat_center);
          printf("long center is %f\n", long_center);
          printf("screen width is %li\n", screen_width);
          printf("screen height is %li\n", screen_height);
          printf("map width is %f\n", map_width);
          printf("map height is %f\n", map_height);
          printf("fileimg is %s\n", fileimg);
        }
    }

    if (terraserver_flag && !tiger_flag) {
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
        "\'http://terraservice.net/download.ashx?t=1\046s=%d\046x=%d\046y=%d\046z=%d\046w=%d\046h=%d\'",
             t_zoom, url_e, url_n, z, geo_image_width, geo_image_height);
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
        printf("X tiepoint width: %ld\n", tp_c_dx);
        printf("Y tiepoint width: %ld\n", tp_c_dy);
    }

    // Calculate step size per pixel
    map_c_dx = ((double) tp_c_dx / abs(tp[1].img_x - tp[0].img_x));
    map_c_dy = ((double) tp_c_dy / abs(tp[1].img_y - tp[0].img_y));

    // Scaled screen step size for use with XFillRectangle below
    scr_dx = (int) (map_c_dx / scale_x) + 1;
    scr_dy = (int) (map_c_dy / scale_y) + 1;

    if (debug_level & 512) {
        printf ("\nImage: %s\n", file);
        printf ("Image size %d %d\n", geo_image_width, geo_image_height);
        printf ("XX: %ld YY:%ld Sx %f %d Sy %f %d\n", map_c_L, map_c_T, map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));
    }

    // calculate top left map corner from tiepoints
    if (tp[0].img_x != 0) {
        tp[0].x_long -= (tp[0].img_x * map_c_dx);   // map left edge longitude
        map_c_L = tp[0].x_long - x_long_offset;     // delta ??
        tp[0].img_x = 0;
        if (debug_level & 512)
            printf("Translated tiepoint_0 x: %d\t%lu\n", tp[0].img_x, tp[0].x_long);
    }
    if (tp[0].img_y != 0) {
        tp[0].y_lat -= (tp[0].img_y * map_c_dy);    // map top edge latitude
        map_c_T = tp[0].y_lat - y_lat_offset;
        tp[0].img_y = 0;
        if (debug_level & 512)
            printf("Translated tiepoint_0 y: %d\t%lu\n", tp[0].img_y, tp[0].y_lat);
    }

    // calculate bottom right map corner from tiepoints
    // map size is geo_image_width / geo_image_height
    if (tp[1].img_x != (geo_image_width - 1) ) {
        tp[1].img_x = geo_image_width - 1;
        tp[1].x_long = tp[0].x_long + (tp[1].img_x * map_c_dx); // right
        if (debug_level & 512)
            printf("Translated tiepoint_1 x: %d\t%lu\n", tp[1].img_x, tp[1].x_long);
    }
    if (tp[1].img_y != (geo_image_height - 1) ) {
        tp[1].img_y = geo_image_height - 1;
        tp[1].y_lat = tp[0].y_lat + (tp[1].img_y * map_c_dy);   // bottom
        if (debug_level & 512)
            printf("Translated tiepoint_1 y: %d\t%lu\n", tp[1].img_y, tp[1].y_lat);
    }

    // Check whether map is inside our current view
    //                bottom        top    left        right
    if (!map_visible (tp[1].y_lat, tp[0].y_lat, tp[0].x_long, tp[1].x_long)) {
        if (debug_level & 16) {
            printf ("Map not in current view, skipping: %s\n", file);
            printf ("\nImage: %s\n", file);
            printf ("Image size %d %d\n", geo_image_width, geo_image_height);
            printf ("XX: %ld YY:%ld Sx %f %d Sy %f %d\n", map_c_L, map_c_T, map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));
        }
        return;            /* Skip this map */
    } else if (debug_level & 16) {
        printf ("Loading imagemap: %s\n", file);
        printf ("\nImage: %s\n", file);
        printf ("Image size %d %d\n", geo_image_width, geo_image_height);
        printf ("XX: %ld YY:%ld Sx %f %d Sy %f %d\n", map_c_L, map_c_T, map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));
    }

    xastir_snprintf(map_it, sizeof(map_it), langcode ("BBARSTA028"), filenm);
    statusline(map_it,0);       // Loading ...

    atb.valuemask = 0;

    (void)get_map_dir (file);


//WE7U
// Best here would be to add the process ID or user ID to the filename
// (to keep the filename distinct for different users), and to check
// the timestamp on the map file.  If it's older than xx minutes, go
// get another one.  Make sure to delete the temp files when closing
// Xastir.  It'd probably be good to check for old files and delete
// them when starting Xastir as well.

    // Check to see if we have to use "wget" to go get an internet map
    if ( (strncasecmp ("http", fileimg, 4) == 0)
            || (strncasecmp ("ftp", fileimg, 3) == 0)
            || (tiger_flag)
            || (findu_flag)
            || (terraserver_flag) ) {
#ifdef HAVE_IMAGEMAGICK
        char *ext;

        if (debug_level & 16)
            printf("ftp or http file: %s\n", fileimg);

        if (findu_flag)
            ext = "log";
        else if (tiger_flag)
            ext = "gif";
        else if (terraserver_flag)
            ext = "jpg";
        else
            ext = get_map_ext(fileimg); // Use extension to determine image type

        xastir_snprintf(local_filename, sizeof(local_filename), "/var/tmp/xastir_%s_map.%s",
                username,ext);

        xastir_snprintf(tempfile, sizeof(tempfile),
                "wget -S -N -t 1 -T 30 -O %s %s 2> /dev/null\n",
                local_filename,
                fileimg);

        if (debug_level & 16)
            printf("%s",tempfile);

        if ( system(tempfile) ) {   // Go get the file
            printf("Couldn't download the image\n");
            return;
        }

        // Set permissions on the file so that any user can overwrite it.
        chmod(local_filename, 0666);

        // We now re-use the "file" variable.  It'll hold the
        //name of the map file now instead of the .geo file.
        strcpy(file,local_filename);  // Tell ImageMagick where to find it
#endif  // HAVE_IMAGEMAGICK

    } else {
        //printf("Not ftp or http file\n");

        // We now re-use the "file" variable.  It'll hold the
        //name of the map file now instead of the .geo file.
        strcat (file, fileimg);
    }

    //printf("File = %s\n",file);

//WE7U
// The status line is not updated yet, probably 'cuz we're too busy
// getting the map in this thread and aren't redrawing?


// Here we do the findu stuff, if the findu_flag is set.  Else we do an imagemap.
if (findu_flag) {
    // We have the log data we're interested in within the /var/tmp/xastir_<username>_map.log file.
    // Cause that file to be read by the "File->Open Log File" routine.  HTML
    // tags will be ignored just fine.
    read_file_ptr = fopen(file, "r");
    if (read_file_ptr != NULL)
        read_file = 1;
    else
        printf("Couldn't open file: %s\n", file);

    return;
}


#ifdef HAVE_IMAGEMAGICK
// Need to figure out the visual and color depth/type here so
// that we can draw the right colors on the screen.

    if (debug_level & 16) {
        visual_list = XGetVisualInfo ( XtDisplay(w), VisualNoMask, &visual_template, &visuals_matched );
        if (visuals_matched) {
            int ii;

            printf("Found %d visuals\n", visuals_matched);
            for (ii = 0; ii < visuals_matched; ii++) {
                printf("\tID:           %ld\n", visual_list[ii].visualid);
                printf("\tScreen:       %d\n",  visual_list[ii].screen);
                printf("\tDepth:        %d\n",  visual_list[ii].depth);
                printf("\tClass:        %d",    visual_list[ii].class);
                switch (visual_list[ii].class) {
                    case 0:
                        printf(",  StaticGray\n");
                        break;
                    case 1:
                        printf(",  GrayScale\n");
                        break;
                    case 2:
                        printf(",  StaticColor\n");
                        break;
                    case 3:
                        printf(",  PseudoColor\n");
                        break;
                    case 4:
                        printf(",  TrueColor\n");
                        break;
                    case 5:
                        printf(",  DirectColor\n");
                        break;
                    default:
                        printf(",  ??\n");
                        break;
                }
                printf("\tClrmap Size:  %d\n", visual_list[ii].colormap_size);
                printf("\tBits per RGB: %d\n", visual_list[ii].bits_per_rgb);
            }
        }
    }


    GetExceptionInfo(&exception);
    image_info=CloneImageInfo((ImageInfo *) NULL);
    (void) strcpy(image_info->filename, file);
    if (debug_level & 16) {
           printf ("Copied %s into image info.\n", file);
           printf ("image_info got: %s\n", image_info->filename);
           printf ("Entered ImageMagick code.\n");
           printf ("Attempting to open: %s\n", image_info->filename);
    }

//WE7U
    // We do a test read first to see if the file exists, so we
    // don't kill Xastir in the ReadImage routine.
    f = fopen (image_info->filename, "r");
    if (f == NULL) {
        if (debug_level & 16)
            printf("File could not be read\n");
        return;
    }
    (void)fclose (f);

    image = ReadImage(image_info, &exception);

    if (image == (Image *) NULL) {
        MagickError(exception.severity, exception.reason, exception.description);
        //printf ("MagickError\n");
        return;
    }

    if (debug_level & 16) 
        printf("Color depth is %i \n", (int)image->depth);

//    if (image->colors == 0) {
//        if (image)
//           DestroyImage(image);
//        if (image_info)
//           DestroyImageInfo(image_info); 
//        return;
//    }

    atb.width = image->columns;
    atb.height = image->rows;

// Do this to minimize the number of colors, does not eliminate distinct colors.
    CompressColormap(image);


//    SyncImage(image);   // Synchronize DirectClass colors to
                        // current PseudoClass colormap.

    //if (AllocateImageColormap(image, image->colors))
        //printf("Colormap Allocated\n");
    //else
        //printf("Colormap Not Allocated\n");

/*    cmap = DefaultColormap(XtDisplay(w), DefaultScreen(XtDisplay(w)));  KD6ZWR - now set in main()*/

    for (l = 0; l < (int)image->colors; l++) {
        // Need to check how to do this for ANY image, as ImageMagick can read in all sorts
        // of image files
        temp_pack = image->colormap[l];
        if (debug_level & 16) 
            printf("Colormap color is %i  %i  %i \n", temp_pack.red, temp_pack.green, temp_pack.blue);


        // Here's a tricky bit:  PixelPacket entries are defined as Quantum's.  Quantum
        // is defined in /usr/include/magick/image.h as either an unsigned short or an
        // unsigned char, depending on what "configure" decided when ImageMagick was installed.
        // We can determine which by looking at MaxRGB or QuantumDepth.
        //
        if (QuantumDepth == 16) {   // Defined in /usr/include/magick/image.h
            if (debug_level & 16)
                printf("Color quantum is [0..65535]\n");
            my_colors[l].red   = (unsigned short)(temp_pack.red  * geotiff_map_intensity);
            my_colors[l].green = (unsigned short)(temp_pack.green* geotiff_map_intensity);
            my_colors[l].blue  = (unsigned short)(temp_pack.blue * geotiff_map_intensity);
        }
        else {  // QuantumDepth = 8
            if (debug_level & 16)
                printf("Color quantum is [0..255]\n");
            my_colors[l].red   = (unsigned short)(temp_pack.red  * geotiff_map_intensity) << 8;
            my_colors[l].green = (unsigned short)(temp_pack.green* geotiff_map_intensity) << 8;
            my_colors[l].blue  = (unsigned short)(temp_pack.blue * geotiff_map_intensity) << 8;
        }


        //  Get the color allocated.  Allocated pixel color is written to my_colors.pixel
        XAllocColor( XtDisplay (w), cmap, &my_colors[l] );
        if (debug_level & 16) 
            printf("Color allocated is %li  %i  %i  %i \n", my_colors[l].pixel, my_colors[l].red, my_colors[l].blue, my_colors[l].green);
    } 

    if (debug_level & 16) {
       printf ("Image size %d %d\n", atb.width, atb.height);
       printf ("Total colors = %d\n", (int)image->colors); 
       printf ("XX: %ld YY:%ld Sx %f %d Sy %f %d\n", map_c_L, map_c_T, map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));

       #if (MagickLibVersion < 0x0540)
           printf ("is Gray Image = %i\n", IsGrayImage(image));
           printf ("is Monochrome Image = %i\n", IsMonochromeImage(image));
           //printf ("is Opaque Image = %i\n", IsOpaqueImage(image));
           //printf ("is PseudoClass = %i\n", IsPseudoClass(image));
       #else
           printf ("is Gray Image = %i\n", IsGrayImage( image, &exception ));
           printf ("is Monochrome Image = %i\n", IsMonochromeImage( image, &exception ));
           //printf ("is Opaque Image = %i\n", IsOpaqueImage( image, &exception ));
           //printf ("is PseudoClass = %i\n", IsPseudoClass( image, &exception ));
       #endif

       printf ("Colorspace = %i\n", image->colorspace); 
       printf ("image matte is %i\n", image->matte);
       if (image->colorspace == UndefinedColorspace)
            printf("Class Type = Undefined\n");
       else if (image->colorspace == RGBColorspace)
            printf("Class Type = RGBColorspace\n");
       else if (image->colorspace == GRAYColorspace)
            printf("Class Type = GRAYColorspace\n");
       else if (image->colorspace == sRGBColorspace)
            printf("Class Type = sRGBColorspace\n");
    }

    // Note that some versions of ImageMagick 5.4 mis-report the
    // version as 5.3.9.  Also note that the API changed in some
    // of the 5.4 versions, and IsPseudoClass() isn't supported
    // in newer versions of the library.
    #if (MagickLibVersion < 0x0539)
    if ( (image->colorspace == RGBColorspace)
            && (IsPseudoClass(image) == 0) ) {
        DirectClass = 1;
    }
    #else
    #endif

#else   // HAVE_IMAGEMAGICK

    // We don't have ImageMagick libs compiled in, so use the
    // XPM library instead.

    /*  XpmReadFileToImage is the call we wish to avoid if at all
     *  possible.  On large images this can take quite a while.  We
     *  check above to see whether the image is inside our viewport,
     *  and if not we skip loading the image.
     */
    if (! XpmReadFileToImage (XtDisplay (w), file, &xi, NULL, &atb) == XpmSuccess) {
        printf ("ERROR loading %s\n", file);
        if (xi)
            XDestroyImage (xi);
        return;
    }
    if (debug_level & 16) {
        printf ("Image size %d %d\n", (int)atb.width, (int)atb.height);
        printf ("XX: %ld YY:%ld Sx %f %d Sy %f %d\n", map_c_L, map_c_T, map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));
    }

#endif  // HAVE_IMAGEMAGICK

//if (image->colors == 0) {
//    if (image)
//       DestroyImage(image);
//    if (image_info)
//       DestroyImageInfo(image_info); 
//   return;
//}

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
//    printf("map top: %ld bottom: %ld\n",tp[0].y_lat,tp[1].y_lat);
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
//                printf("map: min %ld ctr %ld max %ld, c_dx %ld, c_x_min %ld, c_y_min %ld\n",map_x_min,map_x_ctr,map_x_max,(long)c_dx,c_x_min,c_y_min);
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
//                            printf("dist: %ldkm, ew_ofs: %ldkm, dy: %ldkm\n",(long)(1.852*dist),(long)(1.852*ew_ofs),(long)(1.852*c_y_a/6000.0));
//                            printf("  corrfact: %f, mapy0: %ld, mapy: %ld\n",corrfact,map_y_0,map_y);
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
//                printf("map: x %ld y %ld scr: x %ld y %ld dx %d, dy %d\n",map_x,map_y,scr_x,scr_y,scr_dx,scr_dy);
//                printf("color: %ld\n",XGetPixel (xi, map_x, map_y));
//                // 65529
//            }

                    // now copy a pixel from the map image to the screen
#ifdef HAVE_IMAGEMAGICK
                    pixel_pack = GetOnePixel(image, map_x, map_y);
                    index_pack = GetIndexes(image);

                    // Check for RGBColorspace
                    if (DirectClass) {
                        my_colors[0].red  = pixel_pack.red  * geotiff_map_intensity * 256;
                        my_colors[0].green= pixel_pack.green* geotiff_map_intensity * 256;
                        my_colors[0].blue = pixel_pack.blue * geotiff_map_intensity * 256;
                        //  Get the color allocated.  Allocated pixel color is written to my_colors.pixel
                        XAllocColor( XtDisplay (w), cmap, &my_colors[0] );
 
                        //XSetForeground (XtDisplay(w), gc, pixel_pack);
                        XSetForeground (XtDisplay(w), gc, my_colors[0].pixel);
                    }
                    else {
                        XSetForeground (XtDisplay(w), gc, my_colors[index_pack[0]].pixel); 
                    }

#else   // HAVE_IMAGEMAGICK

//                if (scr_x > scr_xp+1) {         // we want to interpolate
//                    for (x=scr_xp+1;x<=scr_x;x++) {
//    unsigned long color;
//                        color = XGetPixel (xi, x, map_y);
//                        (void)XSetForeground (XtDisplay (w), gc, color);
//                    }
//                } else
                    (void)XSetForeground (XtDisplay (w), gc, XGetPixel (xi, map_x, map_y));
                    
#endif  // HAVE_IMAGEMAGICK
                    (void)XFillRectangle (XtDisplay (w),pixmap,gc,scr_x,scr_y,scr_dx,scr_dy);
//--------------------------
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

#endif // NO_GRAPHICS

}





#ifdef HAVE_GEOTIFF
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
    char fgd_file[2000];        /* Complete path/name of .fgd file */
    FILE *fgd;                  /* Filehandle of .fgd file */
    char line[400];             /* One line from .fgd file */
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
        printf("%s\n",fgd_file);

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
            printf("%s\n",fgd_file);

        fgd = fopen (fgd_file, "r");
    }

    if (fgd != NULL)
    {
        while ( ( !feof (fgd) ) && ( num_coordinates < 4 ) )
        {
            get_line (fgd, line, 399);

            if (*f_west_bounding == 0.0)
            {
                if ( (ptr = strstr(line, "WEST BOUNDING COORDINATE:") ) != NULL)
                {
                    sscanf (ptr + 25, " %f", f_west_bounding);
                    if (debug_level & 512)
                        printf("West Bounding:  %f\n",*f_west_bounding);
                    num_coordinates++;
                }
            }

            else if (*f_east_bounding == 0.0)
            {
                if ( (ptr = strstr(line, "EAST BOUNDING COORDINATE:") ) != NULL)
                {
                    sscanf (ptr + 25, " %f", f_east_bounding);
                    if (debug_level & 512)
                        printf("East Bounding:  %f\n",*f_east_bounding);
                    num_coordinates++;
                }
            }

            else if (*f_north_bounding == 0.0)
            {
                if ( (ptr = strstr(line, "NORTH BOUNDING COORDINATE:") ) != NULL)
                {
                    sscanf (ptr + 26, " %f", f_north_bounding);
                    if (debug_level & 512)
                        printf("North Bounding: %f\n",*f_north_bounding);
                    num_coordinates++;
                }
            }

            else if (*f_south_bounding == 0.0)
            {
                if ( (ptr = strstr(line, "SOUTH BOUNDING COORDINATE:") ) != NULL)
                {
                    sscanf (ptr + 26, " %f", f_south_bounding);
                    if (debug_level & 512)
                        printf("South Bounding: %f\n",*f_south_bounding);
                    num_coordinates++;
                }
            }

        }
        fclose (fgd);
    }
    else
    {
        if (debug_level & 512)
            printf("Couldn't open '.fgd' file, assuming no map collar to chop %s\n",
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
        printf("Couldn't find 4 bounding coordinates in '.fgd' file, map %s\n",
                tif_filename);
        return(0);
    }


    if (debug_level & 512) {
        printf("%f %f %f %f\n",
        *f_south_bounding,
        *f_north_bounding,
        *f_west_bounding,
        *f_east_bounding);
    }

    return(1);    /* Successful */
}





/***********************************************************
 * datum_shift_to_wgs84()
 *
 * Attempt to convert from whatever datum the image is in
 * to WGS84 datum (the normal APRS map datum).
 *
 * TODO:  Generalize this code to take a pointer to the
 * data area and a count of points to convert.
 ***********************************************************/
int datum_shift_to_wgs84 (  float* f_west_bounding,
                            float* f_east_bounding,
                            float* f_north_bounding,
                            float* f_south_bounding,
                            char* original_datum,
                            geocode_t datum )
{
    /* For the moment we'll assume that NAD27 is the initial datum
     * Note that some USGS DRG maps are NAD83.  Will need a large
     * number of possible datum translations for world-wide coverage.
     * I'm not currently looking at the "original_datum" or "datum"
     * input parameters.  They're for future expansion.
     * WE7U
     */

    /* Here is the datum definition that we are translating from */
    static char* src_parms[] =
    {
        "proj=latlong",     /* Might change based on geotiff file */
        "datum=NAD27",      /* Needs to be "original_datum" */
    };

    /* Here is the datum definition that we want to translate to */
    static char* dest_parms[] =
    {
        "proj=latlong",
        "datum=WGS84",
    };

    PJ *src, *dest;
    double* x_ptr;
    double* y_ptr;
    double* z_ptr;
    long point_count = 2l;  /* That's an 'L', not a '1' */
    int point_offset = 1;
    double x[2];
    double y[2];
    double z[2];
    int status;
  

    x_ptr = x;
    y_ptr = y;
    z_ptr = z;


    z[0] = (double)0.0;
    z[1] = (double)0.0;


    if ( ! (src  = pj_init(sizeof(src_parms) /sizeof(char *), src_parms )) )
    {
        printf("datum_shift_to_wgs84: Initialization failed.\n");
        if (src)
            printf("Source: %s\n", pj_strerrno((int)src) );
        return(0);
    }


    if ( ! (dest = pj_init(sizeof(dest_parms)/sizeof(char *), dest_parms)) )
    {
        printf("datum_shift_to_wgs84: Initialization failed.\n");
        if (dest)
            printf("Destination: %s\n", pj_strerrno((int)dest) );
        return(0);
    }


    y[0] = (double)( DEG_TO_RAD * *f_north_bounding );
    x[0] = (double)( DEG_TO_RAD * *f_west_bounding );

    y[1] = (double)( DEG_TO_RAD * *f_south_bounding );
    x[1] = (double)( DEG_TO_RAD * *f_east_bounding );


    /*
     * This call seems to fail the first time it is used, quite
     * often other times as well.  Try it again if it fails the
     * first time.  The datums appear to be defined in pj_datums.c
     * in the proj.4 source code.  Currently defined datums are:
     * WGS84, GGRS87, NAD83, NAD27.
     */
    status = pj_transform( src, dest, point_count, point_offset, x_ptr, y_ptr, z_ptr);
    if (status)
    {
        status = pj_transform( src, dest, point_count, point_offset, x_ptr, y_ptr, z_ptr);
        if (status)
        {
            printf( "datum_shift_to_wgs84: Non-zero status from pj_transform: %d\n",status );
            printf( "datum_shift_to_wgs84: %s\n", pj_strerrno(status) );

            // May or may not be a good idea to skip this, but we'll try to recover.
            // Datum translation failed for some reason, but let's try to load the
            // maps anyway.
            //return(0);
        }
    }


    y[0] = RAD_TO_DEG * y[0];
    x[0] = RAD_TO_DEG * x[0];
    y[1] = RAD_TO_DEG * y[1];
    x[1] = RAD_TO_DEG * x[1];



    if (debug_level & 512)
        printf( "Datum shifted values:  %f\t%f\t%f\t%f\n",
                x[0],
                y[0],
                x[1],
                y[1] );


    /* Free up memory that we used */
    pj_free(src);
    pj_free(dest);


    /* Plug our new values back in */
    *f_north_bounding = (float)y[0];
    *f_west_bounding = (float)x[0];

    *f_south_bounding = (float)y[1];
    *f_east_bounding = (float)x[1];


    return(1);
}





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
void draw_geotiff_image_map (Widget w, char *dir, char *filenm)
{
    char file[1000];            /* Complete path/name of image file */
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
    int    bytesPerRow;            /* Bytes per scanline row of tiff file */
    GTIFDefn defn;              /* Stores geotiff details */
    u_char *imageMemory;        /* Fixed pointer to same memory area */
    uint32 row;                 /* My row counter for the loop */
    int num_colors;             /* Number of colors in the geotiff colormap */
    uint16 *red_orig, *green_orig, *blue_orig; /* Used for storing geotiff colors */
    XColor my_colors[256];      /* Used for translating colormaps */
/*    Colormap cmap;  KD6ZWR - now set in main()*/
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
    float f_NW_x_bounding_wgs84 = 0.0;
    float f_NW_y_bounding_wgs84 = 0.0;

    unsigned long NE_x_bounding_wgs84 = 0;
    unsigned long NE_y_bounding_wgs84 = 0;
    float f_NE_x_bounding_wgs84 = 0.0;
    float f_NE_y_bounding_wgs84 = 0.0;

    unsigned long SW_x_bounding_wgs84 = 0;
    unsigned long SW_y_bounding_wgs84 = 0;
    float f_SW_x_bounding_wgs84 = 0.0;
    float f_SW_y_bounding_wgs84 = 0.0;

    unsigned long SE_x_bounding_wgs84 = 0;
    unsigned long SE_y_bounding_wgs84 = 0;
    float f_SE_x_bounding_wgs84 = 0.0;
    float f_SE_y_bounding_wgs84 = 0.0;

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
    char map_it[300];           /* Used to hold filename for status line */
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
        printf ("%s/%s\n", dir, filenm);


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
    if (have_fgd)   /* Must be a USGS file */
    {
        crop_it = 1;        /* The map collar needs to be cropped */

        convert_to_xastir_coordinates(  &west_bounding,
                                        &north_bounding,
                                        f_west_bounding,
                                        f_north_bounding );


        convert_to_xastir_coordinates(  &east_bounding,
                                        &south_bounding,
                                        f_east_bounding,
                                        f_south_bounding );


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
        if (!map_visible( south_bounding + 1000,
                             north_bounding - 1000,
                             west_bounding - 1000,
                             east_bounding + 1000 ) )
        {
            if (debug_level & 16) {
                printf ("Map not within current view.\n");
                printf ("Skipping map: %s\n", file);
            }

            /* Map isn't inside our current view.  We're done.
             * Free any memory used and return.
             */
            return;                     /* Skip this map */
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
        printf ("This file is too new for me\n");
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
        printf("No width tag found in file, setting it to 5493\n");
    }

    if ( !TIFFGetField (tif, TIFFTAG_IMAGELENGTH, &height) ) {
        height = 6840;
        printf("No height tag found in file, setting it to 6840\n");
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
            printf("\nCorner Coordinates:\n");

        /* Find lat/lon for NW corner of image */
        xxx = 0.0;
        yyy = 0.0;
        if ( GTIFImageToPCS( gtif, &xxx, &yyy ) )   // Do all 4 of these in one call?
        {
            if (debug_level & 16) {
                printf( "%-13s ", "Upper Left" );
                printf( "(%11.3f,%11.3f)\n", xxx, yyy );
            }
        }
        if ( GTIFProj4ToLatLong( &defn, 1, &xxx, &yyy ) )   // Do all 4 of these in one call?
        {
            if (debug_level & 16) {
                printf( "  (%s,", GTIFDecToDMS( xxx, "Long", 2 ) );
                printf( "%s)\n", GTIFDecToDMS( yyy, "Lat", 2 ) );
                printf("%f  %f\n", xxx, yyy);
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
                printf( "%-13s ", "Lower Right" );
                printf( "(%11.3f,%11.3f)\n", xxx, yyy );
            }
        }
        if ( GTIFProj4ToLatLong( &defn, 1, &xxx, &yyy ) )
        {
            if (debug_level & 16) {
                printf( "  (%s,", GTIFDecToDMS( xxx, "Long", 2 ) );
                printf( "%s)\n", GTIFDecToDMS( yyy, "Lat", 2 ) );
                printf("%f  %f\n", xxx, yyy);
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
                printf( "%-13s ", "Lower Right" );
                printf( "(%11.3f,%11.3f)\n", xxx, yyy );
            }
        }
        if ( GTIFProj4ToLatLong( &defn, 1, &xxx, &yyy ) )
        {
            if (debug_level & 16) {
                printf( "  (%s,", GTIFDecToDMS( xxx, "Long", 2 ) );
                printf( "%s)\n", GTIFDecToDMS( yyy, "Lat", 2 ) );
                printf("%f  %f\n", xxx, yyy);
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
                printf( "%-13s ", "Lower Right" );
                printf( "(%11.3f,%11.3f)\n", xxx, yyy );
            }
        }
        if ( GTIFProj4ToLatLong( &defn, 1, &xxx, &yyy ) )
        {
            if (debug_level & 16) {
                printf( "  (%s,", GTIFDecToDMS( xxx, "Long", 2 ) );
                printf( "%s)\n", GTIFDecToDMS( yyy, "Lat", 2 ) );
                printf("%f  %f\n", xxx, yyy);
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
    //     printf( "GeogGeodeticDatumGeoKey: %d\n", datum );


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
        printf("Tiepoints:\n");
        if ( TIFFGetField( tif, TIFFTAG_GEOTIEPOINTS, &qty, &GeoTie ) ) {
            for ( i = 0; i < qty; i++ ) {
                printf( "%f\n", *(GeoTie + i) );
            }
        }
    }
    */


    /* Get the geotiff horizontal datum name */
    if ( defn.Datum != 32767 ) {
        GTIFGetDatumInfo( defn.Datum, &datum_name, NULL );
        if (debug_level & 16)
            printf("Datum: %d/%s\n", defn.Datum, datum_name );
    }


    /*
     * Perform a datum shift on the bounding coordinates before we
     * check whether the map is inside our viewport.  At the moment
     * this is still hard-coded to NAD27 datum.  If the map is already
     * in WGS84 or NAD83 datum, skip the datum conversion code.
     */
    if (   (defn.Datum != 6030)     /* DatumE_WGS84 */
        && (defn.Datum != 6326)     /*  Datum_WGS84 */
        && (defn.Datum != 6269) )   /* Datum_North_American_Datum_1983 */
    {
        if (debug_level & 16)
            printf("***** Attempting Datum Conversions\n");

        // Change datum_shift to use arrays and counts and make
        // only one call for all 4 corners
        if (   (! datum_shift_to_wgs84 ( &f_NW_x_bounding_wgs84,
                                        &f_NE_x_bounding_wgs84,
                                        &f_NW_y_bounding_wgs84,
                                        &f_NE_y_bounding_wgs84,
                                        datum_name,
                                        defn.Datum) )
            || (! datum_shift_to_wgs84 ( &f_SW_x_bounding_wgs84,
                                        &f_SE_x_bounding_wgs84,
                                        &f_SW_y_bounding_wgs84,
                                        &f_SE_y_bounding_wgs84,
                                        datum_name,
                                        defn.Datum) ) )
        {
            /* Problem doing the datum shift */
            printf("Problem with datum shift.  Perhaps that conversion is not implemented?\n");
            /*
            GTIFFree (gtif);
            XTIFFClose (tif);
            return;
            */
        }
    }
    else
        if (debug_level & 16)
            printf("***** Skipping Datum Conversion\n");


    /*
     * Convert new datum-translated bounding coordinates to the
     * Xastir coordinate system.
     * convert_to_xastir_coordinates( x,y,longitude,latitude )
     */
    // NW corner
    convert_to_xastir_coordinates(  &NW_x_bounding_wgs84,
                                    &NW_y_bounding_wgs84,
                                    f_NW_x_bounding_wgs84,
                                    f_NW_y_bounding_wgs84 );

    // NE corner
    convert_to_xastir_coordinates(  &NE_x_bounding_wgs84,
                                    &NE_y_bounding_wgs84,
                                    f_NE_x_bounding_wgs84,
                                    f_NE_y_bounding_wgs84 );

    // SW corner
    convert_to_xastir_coordinates(  &SW_x_bounding_wgs84,
                                    &SW_y_bounding_wgs84,
                                    f_SW_x_bounding_wgs84,
                                    f_SW_y_bounding_wgs84 );

    // SE corner
    convert_to_xastir_coordinates(  &SE_x_bounding_wgs84,
                                    &SE_y_bounding_wgs84,
                                    f_SE_x_bounding_wgs84,
                                    f_SE_y_bounding_wgs84 );


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

    // bottom top left right
    if (!map_visible( south_bounding_wgs84,
                         north_bounding_wgs84,
                         west_bounding_wgs84,
                         east_bounding_wgs84 ) )
    {
        if (debug_level & 16) {
            printf ("Map not within current view.\n");
            printf ("Skipping map: %s\n", file);
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
    if ( !TIFFGetField (tif, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip) ) {
        rowsPerStrip = 1;
        printf("No rowsPerStrip tag found in file, setting it to 1\n");
    }

    if ( !TIFFGetField (tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample) ) {
        bitsPerSample = 8;
        printf("No bitsPerSample tag found in file, setting it to 8\n");
    }

    if ( !TIFFGetField (tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel) ) {
        samplesPerPixel = 1;
        printf("No samplesPerPixel tag found in file, setting it to 1\n");
    }

    if ( !TIFFGetField (tif, TIFFTAG_PLANARCONFIG, &planarConfig) ) {
        planarConfig = 1;
        printf("No planarConfig tag found in file, setting it to 1\n");
    }


    if (debug_level & 16) {
        printf ("            Width: %ld\n", width);
        printf ("           Height: %ld\n", height);
        printf ("   Rows Per Strip: %ld\n", rowsPerStrip);
        printf ("  Bits Per Sample: %d\n", bitsPerSample);
        printf ("Samples Per Pixel: %d\n", samplesPerPixel);
        printf ("    Planar Config: %d\n", planarConfig);
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
        printf("*** geoTIFF file %s is not in the proper format.\n", file);
        printf("*** Please reformat it and try again.\n");
        XTIFFClose(tif);
        return;
    }


    if (debug_level & 16)
        printf ("Loading geoTIFF map: %s\n", file);


    /* Put "Loading..." message on status line */
    xastir_snprintf(map_it, sizeof(map_it), langcode ("BBARSTA028"), filenm);
    statusline(map_it,0);       // Loading ...


    /*
     * Snag the original map colors out of the colormap embedded
     * inside the tiff file.
     */
    if (!TIFFGetField(tif, TIFFTAG_COLORMAP, &red_orig, &green_orig, &blue_orig))
    {
        TIFFError(TIFFFileName(tif), "Missing required \"Colormap\" tag");
        GTIFFree (gtif);
        XTIFFClose (tif);
        return;
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
    //            printf("   %5u: %5u %5u %5u\n",
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
            printf("\nNW neat-line corner = %f\t%f\n",
                    f_NW_x_bounding,
                    f_NW_y_bounding);

        xxx = (double)f_NW_x_bounding;
        yyy = (double)f_NW_y_bounding;

        /* Convert lat/long to projected coordinates */
        if ( GTIFProj4FromLatLong( &defn, 1, &xxx, &yyy ) )     // Do all 4 in one call?
        {
            if (debug_level & 16)
                printf("%11.3f,%11.3f\n", xxx, yyy);

            /* Convert from PCS coordinates to image pixel coordinates */
            if ( GTIFPCSToImage( gtif, &xxx, &yyy ) )           // Do all 4 in one call?
            {
                if (debug_level & 16)
                    printf("X/Y Pixels: %f, %f\n", xxx, yyy);

                NW_x = (int)(xxx + 0.5);    /* Tricky way of rounding */
                NW_y = (int)(yyy + 0.5);    /* Tricky way of rounding */

                if (debug_level & 16)
                    printf("X/Y Pixels: %d, %d\n", NW_x, NW_y);
            }
        }
        else
            printf("Problem in translating\n");


        if (debug_level & 16)
            printf("NE neat-line corner = %f\t%f\n",
                    f_NE_x_bounding,
                    f_NE_y_bounding);

        xxx = (double)f_NE_x_bounding;
        yyy = (double)f_NE_y_bounding;

        /* Convert lat/long to projected coordinates */
        if ( GTIFProj4FromLatLong( &defn, 1, &xxx, &yyy ) )
        {
            if (debug_level & 16)
                printf("%11.3f,%11.3f\n", xxx, yyy);

            /* Convert from PCS coordinates to image pixel coordinates */
            if ( GTIFPCSToImage( gtif, &xxx, &yyy ) )
            {
                if (debug_level & 16)
                    printf("X/Y Pixels: %f, %f\n", xxx, yyy);

                NE_x = (int)(xxx + 0.5);    /* Tricky way of rounding */
                NE_y = (int)(yyy + 0.5);    /* Tricky way of rounding */

                if (debug_level & 16)
                    printf("X/Y Pixels: %d, %d\n", NE_x, NE_y);
            }
        }
        else
            printf("Problem in translating\n");


        if (debug_level & 16)
            printf("SW neat-line corner = %f\t%f\n",
                    f_SW_x_bounding,
                    f_SW_y_bounding);

        xxx = (double)f_SW_x_bounding;
        yyy = (double)f_SW_y_bounding;

        /* Convert lat/long to projected coordinates */
        if ( GTIFProj4FromLatLong( &defn, 1, &xxx, &yyy ) )
        {
            if (debug_level & 16)
                printf("%11.3f,%11.3f\n", xxx, yyy);

            /* Convert from PCS coordinates to image pixel coordinates */
            if ( GTIFPCSToImage( gtif, &xxx, &yyy ) )
            {
                if (debug_level & 16)
                    printf("X/Y Pixels: %f, %f\n", xxx, yyy);

                SW_x = (int)(xxx + 0.5);    /* Tricky way of rounding */
                SW_y = (int)(yyy + 0.5);    /* Tricky way of rounding */

                if (debug_level & 16)
                    printf("X/Y Pixels: %d, %d\n", SW_x, SW_y);
            }
        }
        else
            printf("Problem in translating\n");


        if (debug_level & 16)
            printf("SE neat-line corner = %f\t%f\n",
                    f_SE_x_bounding,
                    f_SE_y_bounding);

        xxx = (double)f_SE_x_bounding;
        yyy = (double)f_SE_y_bounding;

        /* Convert lat/long to projected coordinates */
        if ( GTIFProj4FromLatLong( &defn, 1, &xxx, &yyy ) )
        {
            if (debug_level & 16)
                printf("%11.3f,%11.3f\n", xxx, yyy);

        /* Convert from PCS coordinates to image pixel coordinates */
        if ( GTIFPCSToImage( gtif, &xxx, &yyy ) )
        {
            if (debug_level & 16)
                printf("X/Y Pixels: %f, %f\n", xxx, yyy);

            SE_x = (int)(xxx + 0.5);    /* Tricky way of rounding */
            SE_y = (int)(yyy + 0.5);    /* Tricky way of rounding */

            if (debug_level & 16)
                printf("X/Y Pixels: %d, %d\n", SE_x, SE_y);
            }
        }
        else
            printf("Problem in translating\n");
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


    if (debug_level & 16) {
        printf("Crop points (pixels):\n");
        printf("Top: %d\tBottom: %d\tLeft: %d\tRight: %d\n",
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

/*    cmap = DefaultColormap( XtDisplay (w), DefaultScreen( XtDisplay (w) ) );  KD6ZWR - now set in main()*/
    {
        int l;
        // float geotiff_map_intensity = 1.00;    // Change this to reduce the
                                    // intensity of the map colors

        for (l = 0; l < num_colors; l++)
        {
            my_colors[l].red   =   (uint16)(red_orig[l] * geotiff_map_intensity);
            my_colors[l].green = (uint16)(green_orig[l] * geotiff_map_intensity);
            my_colors[l].blue  =  (uint16)(blue_orig[l] * geotiff_map_intensity);
      
            XAllocColor( XtDisplay (w), cmap, &my_colors[l] );
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
        printf("\nInitial Bytes Per Row: %d\n", bytesPerRow);
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

        if (debug_level & 16)
             printf("xastir_left_x_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
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

        if (debug_level & 16)
             printf("xastir_left_y_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
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

        if (debug_level & 16)
            printf("xastir_right_x_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
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

        if (debug_level & 16)
            printf("xastir_right_y_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
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
        printf(" Left x increments: %f %f\n", xastir_left_x_increment, left_x_increment);
        printf(" Left y increments: %f %f\n", xastir_left_y_increment, left_y_increment);
        printf("Right x increments: %f %f\n", xastir_right_x_increment, right_x_increment);
        printf("Right y increments: %f %f\n", xastir_right_y_increment, right_y_increment);
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
            printf("xastir_top_y_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
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
            printf("xastir_bottom_y_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
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
            printf("PixelScale: %f %f %f\n",
                *PixelScale,
                *(PixelScale + 1),
                *(PixelScale + 2) );
        }
        else {
            printf("No PixelScale tag found in file\n");
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
        printf("Size: x %ld, y %ld\n", scale_x,scale_y);


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
        printf("SkipRows: %d\n", SkipRows);

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
        //printf("row_offset: %d\n", row_offset);


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
        //  printf("Left: %ld  Right:  %ld\n",
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

        current_line_width = current_right - current_left + 1;  // Pixels


        // if (debug_level & 16)
        //     printf("Left: %ld  Right: %ld  Width: %ld\n",
        //         current_left,
        //         current_right, current_line_width);


        // Compute original pixel size in Xastir coordinates.  Note
        // that this can change for each scanline in a USGS geoTIFF.

        // (Xastir right - Xastir left) / width-of-line
        // Need the "1.0 *" or the math will be incorrect (won't be a float)
        stepw = (float)( (current_xastir_right - current_xastir_left)
                      / (1.0 * (current_right - current_left) ) );



        // if (debug_level & 16)
        //     printf("\t\t\t\t\t\tPixel Width: %f\n",stepw);

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

                //printf("Column Offset: %ld\n", column_offset);  // Pixels
                //printf("Current Left: %ld\n", current_left);    // Pixels

                xastir_current_x = (unsigned long)
                                    current_xastir_left
                                    + (stepw * column_offset);    // In Xastir coordinates

                // Left line y value minus
                // avg y-increment per scanline * avg y-increment per x-pixel * column_offset
                xastir_total_y = (unsigned long)
                                  ( xastir_current_y
                                - ( total_avg_y_increment * column_offset ) );

                //printf("Xastir current: %ld %ld\n", xastir_current_x, xastir_current_y);


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
                            *(imageMemory + column) = 0x01;     // Change to White
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
                    sxx = (long)( ( (xastir_current_x - x_long_offset) / scale_x) + 0.5);
                    syy = (long)( ( (xastir_total_y   - y_lat_offset ) / scale_y) + 0.5);

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
        printf ("%d rows read in\n", (int) row);

    /* We're finished with the geoTIFF key parser, so get rid of it */
    GTIFFree (gtif);

    /* Close the TIFF file descriptor */
    XTIFFClose (tif);

    // Close the filehandles that are left open after the
    // four GTIFImageToPCS calls.
    //(void)CSVDeaccess(NULL);
}
#endif /* HAVE_GEOTIFF */





// Test map visibility (on screen)
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
      printf("x_long_offset: %ld, y_lat_offset: %ld, max_x_long_offset: %ld, max_y_lat_offset: %ld\n",
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





void draw_palm_image_map(Widget w, char *dir, char *filenm, int destination_pixmap) {

#pragma pack(1)
    struct {
            char name[32];
        short file_attributes;
        short version;
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
        short next_vector;
        short left_bounds;
        short right_bounds;
        short top_bounds;
        short bottom_bounds;
        short line_start_x;
        short line_start_y;
    } vector_hdr;

    struct {
        unsigned char next_x;
        unsigned char next_y;
    } vector_point;

    struct {
        short next_label;
        short start_x;
        short start_y;
        char symbol_set;
        char symbol_char;
        char color;
        char treatment;
        short fill;
        char text[20];
    } label_record;     

    FILE *f;
    char filename[200];
    int records, record_count, count;
    int scale;
    long map_left, map_right, map_top, map_bottom, max_x, max_y;
    long record_ptr;
    long line_x, line_y;
    int vector;


    xastir_snprintf(filename, sizeof(filename), "%s/%s", dir, filenm);
    if ((f = fopen(filename, "r")) != NULL) {

        if (debug_level & 1)
            printf("opened file: %s\n", filename);

        fread(&pdb_hdr, sizeof(pdb_hdr), 1, f);
        if (strncmp(pdb_hdr.database_type, "map1", 4) != 0
                || strncmp(pdb_hdr.creator_type, "pAPR", 4) != 0) {
            printf("Not Palm OS Map: %s\n", filename);
            fclose(f);
            return;
        }
        records = ntohs(pdb_hdr.number_of_records);
        fread(&prl, sizeof(prl), 1, f);
        if (debug_level & 512)
            printf("Palm Map: %s, %d records, offset: %8x\n",
                pdb_hdr.name,
                records,
                (unsigned int)ntohl(prl.record_data_offset));

        record_ptr = ftell(f);
        fseek(f, ntohl(prl.record_data_offset), SEEK_SET);
        fread(&pmf_hdr, sizeof(pmf_hdr), 1, f);
        scale = ntohs(pmf_hdr.granularity);
        map_left = ntohl(pmf_hdr.left_bounds);
        map_right = ntohl(pmf_hdr.right_bounds);
        map_top = ntohl(pmf_hdr.top_bounds);
        map_bottom = ntohl(pmf_hdr.bottom_bounds);
        if (debug_level & 512)
            printf("\tLeft %ld, Right %ld, Top %ld, Bottom %ld, %s, Scale %d, %d\n",
                map_left,
                map_right,
                map_top,
                map_bottom,
                pmf_hdr.menu_name,
                scale,
                pmf_hdr.sort_order);

        // DNN: multipy by 10; pocketAPRS corners in tenths of seconds,
        // internal map in hundredths of seconds (was "scale" which was wrong,
        // scale is not used for the map corners)
        // Multipy now so we don't have to do it for every use below...
        map_left = map_left * 10;
        map_right = map_right * 10;
        map_top = map_top * 10;
        map_bottom = map_bottom * 10;
        if (map_onscreen(map_left, map_right, map_top, map_bottom)) {
            max_x = screen_width  + MAX_OUTBOUND;
            max_y = screen_height + MAX_OUTBOUND;
            /* read vectors */
            for (record_count = 2; record_count <= records; record_count++) {
                fseek(f, record_ptr, SEEK_SET);
                fread(&prl, sizeof(prl), 1, f);
                if (debug_level & 512)
                    printf("\tRecord %d, offset: %8x\n",
                        record_count,
                        (unsigned int)ntohl(prl.record_data_offset));

                record_ptr = ftell(f);
                fseek(f, ntohl(prl.record_data_offset), SEEK_SET);
                fread(&record_hdr, sizeof(record_hdr), 1, f);
                if (debug_level & 512)
                    printf("\tType %d, Sub %d, Zoom %d\n",
                        record_hdr.type,
                        record_hdr.sub_type,
                        ntohs(record_hdr.minimum_zoom));

                if (record_hdr.type > 0 && record_hdr.type < 16) {
                    vector = True;
                    while (vector && fread(&vector_hdr, sizeof(vector_hdr), 1, f)) {
                        count = ntohs(vector_hdr.next_vector);
                        if (count && !(count&1)) {
                            line_x = ntohs(vector_hdr.line_start_x);
                            line_y = ntohs(vector_hdr.line_start_y);
                            if (debug_level & 512)
                                printf("\tvector %d, left %d, right %d, top %d, bottom %d, start x %ld, start y %ld\n",
                                    count,
                                    ntohs(vector_hdr.left_bounds),
                                    ntohs(vector_hdr.right_bounds),
                                    ntohs(vector_hdr.top_bounds),
                                    ntohs(vector_hdr.bottom_bounds),
                                    line_x,
                                    line_y);

                            // DNN: Only line_x and line_y are scaled,
                            // not map_left and map_top
                            map_plot (w,
                                max_x,
                                max_y,
                                map_left + (line_x * scale),
                                map_top + (line_y * scale),
                                record_hdr.type,
                                0,
                                destination_pixmap);

                            for (count -= sizeof(vector_hdr); count > 0; count -= sizeof(vector_point)) {
                                fread(&vector_point, sizeof(vector_point), 1, f);
                                if (debug_level & 512)
                                    printf("\tnext x %d, next y %d\n",
                                        vector_point.next_x,
                                        vector_point.next_y);
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
                                    destination_pixmap);
                                        }

                                        // DNN: Only line_x and line_y are scaled,
                                        // not map_left and map_top
                                        map_plot (w,
                                max_x,
                                max_y,
                                map_left + (line_x * scale),
                                map_top + (line_y * scale),
                                0,
                                                0,
                                destination_pixmap);
                                    }
                        else {
                                        vector = False;
                        }
                            }
                        }
                else if (record_hdr.type == 0) {  // We have a label
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
                    while (label && fread(&label_record, sizeof(label_record), 1, f)) {
                        count = ntohs(label_record.next_label);
                        if (count && !(count&1)) {
                            line_x = ntohs(vector_hdr.line_start_x);
                            line_y = ntohs(vector_hdr.line_start_y);
 
                            if (debug_level & 512) {
                                printf("\t%d, %d, %d, %d, %d, %d, 0x%x, %s\n",
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
            fclose(f);
        if (debug_level & 1)
            printf("Closed file\n");
    }
    else {
        printf("Couldn't open file: %s\n", filename);
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
 **********************************************************/
void draw_map (Widget w, char *dir, char *filenm, alert_entry * alert,
                unsigned char alert_color, int destination_pixmap) {
    FILE *f;
    char file[2000];
    char map_it[300];

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

    xastir_snprintf(file, sizeof(file), "%s/%s", dir, filenm);
    ext = get_map_ext (filenm);


    // If alert is non-NULL, then we have a weather alert and we need
    // to call draw_shapefile_map() to light up that area.  If alert
    // is NULL, then we decide here what method to use to draw the
    // map.


    // Check for WX alert/ESRI Shapefile maps first
    if ( (alert != NULL)    // We have an alert!
            || ( (ext != NULL) && ( (strcasecmp(ext,"shp") == 0)
                                 || (strcasecmp(ext,"shx") == 0)
                                 || (strcasecmp(ext,"dbf") == 0) ) ) ) { // Or non-alert shapefile map
#ifdef HAVE_SHAPELIB
        //printf("Drawing shapefile map\n");
        if (alert != NULL) {
            //printf("Alert!\n");
        }
        draw_shapefile_map (w, dir, filenm, alert, alert_color, destination_pixmap);
#endif // HAVE_SHAPELIB
    }


    // .geo image map? (can be one of many formats)
    else if (ext != NULL && strcasecmp (ext, "geo") == 0) {
        draw_geo_image_map (w, dir, filenm);
    }


    // Palm map?
    else if (ext != NULL && strcasecmp (ext, "pdb") == 0) {
        //printf("calling draw_palm_image_map: %s/%s\n", dir, filenm);
        draw_palm_image_map (w, dir, filenm, destination_pixmap);
    }


    // GNIS database file?
    else if (ext != NULL && strcasecmp (ext, "gnis") == 0) {
        draw_gnis_map (w, dir, filenm);
    }


#ifdef HAVE_GEOTIFF
    // USGS DRG geoTIFF map?
    else if (ext != NULL && strcasecmp (ext, "tif") == 0) {
        draw_geotiff_image_map (w, dir, filenm);
    }
#endif // HAVE_GEOTIFF


    // DOS/WinAPRS map?
    else if (ext != NULL && strcasecmp (ext, "MAP") == 0) {
        f = fopen (file, "r");
        if (f != NULL) {
            (void)fread (map_type, 4, 1, f);
            map_type[4] = '\0';
            dos_labels = FALSE;
            points_per_degree = 300;
            if (strtod (map_type, &ptr) > 0.01 && (*ptr == '\0' || *ptr == ' ' || *ptr == ',')) {
                /* DOS-type map header portion of the code */
                int j;

                if (debug_level & 512)
                    printf("DOS Map\n");

//printf("DOS Map\n");

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
#else
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
            } else {    // Windows-type map header portion

                if (debug_level & 512)
                    printf("Windows map\n");

//printf("Windows map\n");

                (void)fread (map_version, 4, 1, f);
                map_version[4] = '\0';

                (void)fread (file_name, 32, 1, f);
                file_name[32] = '\0';

                (void)fread (map_title, 32, 1, f);
                map_title[32] = '\0';
                if (debug_level & 16)
                    printf ("Map Title %s\n", map_title);

                (void)fread (map_creator, 8, 1, f);
                map_creator[8] = '\0';
                if (debug_level & 16)
                    printf ("Map Creator %s\n", map_creator);

                (void)fread (&temp, 4, 1, f);
                creation_date = ntohl (temp);
                if (debug_level & 16)
                    printf ("Creation Date %lX\n", creation_date);

                year = creation_date / 31536000l;
                days = (creation_date - (year * 31536000l)) / 86400l;
                if (debug_level & 16)
                    printf ("year is %ld + days %ld\n", 1904l + year, (long)days);

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

            if (debug_level & 16) {
                printf ("Map Type %s, Version: %s, Filename %s\n", map_type,map_version, file_name);
                printf ("Left Boundary %ld, Right Boundary %ld\n", (long)left_boundary,(long)right_boundary);
                printf ("Top Boundary %ld, Bottom Boundary %ld\n", (long)top_boundary,(long)bottom_boundary);
                printf ("Total vector points %ld, total labels %ld\n",total_vector_points, total_labels);
            }

//            if (alert) {
//                strncpy (alert->filename, filenm, sizeof (alert->filename));
//                strcpy (alert->title, map_title);
//                alert->top_boundary    = top_boundary;
//                alert->bottom_boundary = bottom_boundary;
//                alert->left_boundary   = left_boundary;
//                alert->right_boundary  = right_boundary;
//            }

            in_window = map_onscreen(left_boundary, right_boundary, top_boundary, bottom_boundary);

            if (alert)
                alert->flags[0] = in_window ? 'Y' : 'N';

            if (in_window && !alert) {
                unsigned char last_behavior, special_fill = (unsigned char)FALSE;
                object_behavior = '\0';
                xastir_snprintf(map_it, sizeof(map_it), langcode ("BBARSTA028"), filenm);
                statusline(map_it,0);       // Loading ...
                if (debug_level & 1)
                    printf ("in Boundary %s\n", map_it);

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
                                        map_plot (w, max_x, max_y, (long)x_long_cord, (long)y_lat_cord, (unsigned char)color, 0, destination_pixmap);
                                    }
                                    map_plot (w, max_x, max_y, (long)x_long_cord, (long)y_lat_cord, '\0', 0, destination_pixmap);
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
                                        map_plot (w, max_x, max_y, (long)x_long_cord, (long)y_lat_cord, (unsigned char)color,0, destination_pixmap);
                                    } else if (LongHld == 0 && LatHld == -1) {
                                        dos_labels = (int)TRUE;
                                        map_plot (w, max_x, max_y, (long)x_long_cord, (long)y_lat_cord, '\0', 0, destination_pixmap);
                                    } else {
                                        LatHld = ((double)LatHld * 360000.0) / points_per_degree;
                                        LongHld = ((double)LongHld * 360000.0) / points_per_degree;
                                        x_long_cord = LongHld + left_boundary;
                                        y_lat_cord = LatHld + top_boundary;
                                        map_plot (w, max_x, max_y, (long)x_long_cord, (long)y_lat_cord, (unsigned char)color,0, destination_pixmap);
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
                                        map_plot (w, max_x, max_y, (long)x_long_cord, (long)y_lat_cord, (unsigned char)color,0, destination_pixmap);
                                    } else if (LongHld == 0 && LatHld == -1) {
                                        dos_labels = (int)TRUE;
                                        map_plot (w, max_x, max_y, (long)x_long_cord, (long)y_lat_cord, (unsigned char)color,0, destination_pixmap);
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
                                            map_plot (w, max_x, max_y, (long)x_long_cord, (long)y_lat_cord, (unsigned char)color, 0, destination_pixmap);
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
                            map_plot (w, max_x, max_y, (long)x_long_cord, (long)y_lat_cord, '\0',(long)alert_color, destination_pixmap);
                            //special_fill = TRUE;
                        }
                        map_plot (w, max_x, max_y, (long)x_long_cord, (long)y_lat_cord, vector_start,(long)object_behavior, destination_pixmap);
                    }
                }
                if (alert_color)
                    map_plot (w, max_x, max_y, 0, 0, '\0',special_fill ? (long)0xfd : (long)alert_color, destination_pixmap);
                else
                    map_plot (w, max_x, max_y, 0, 0, (unsigned char)0xff, 0, destination_pixmap);

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
 
//WE7U6
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
                                            printf("Found embedded object: %c %c %c %s\n",symbol_table,symbol_id,symbol_color,label_text);
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
                                            /*printf("Label mag %d mag %d\n",label_mag,(scale_x*2)-1); */
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
//WE7U6
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


//WE7U6
                                // Special strings like: "#123" are rotation factors for labels
                                // in degrees.  This is not documented in the windows-type map
                                // format documents that I could find.
                                if (label_text[0] == '#') {
                                    int i,j;
                                    if (debug_level & 512)
                                        printf("%s\n",label_text);

                                    // Save the rotation factor in "rotation"
                                    for ( i=1; i<4; i++ )
                                        rotation_factor[i-1] = label_text[i];
                                    rotation_factor[3] = '\0';
                                    rotation = atoi(rotation_factor);

                                    // Take rotation factor out of label string
                                    for ( i=4, j=0; i < (int)(strlen(label_text)+1); i++,j++)
                                        label_text[j] = label_text[i];

//WE7U6
                                    //printf("Windows label: %s, rotation factor: %d\n",label_text, rotation);
                                }


                                label_length = (int)strlen (label_text);

                                for (i = (label_length - 1); i > 0; i--) {
                                    if (label_text[i] == ' ')
                                        label_text[i] = '\0';
                                    else
                                        break;
                                }

                                label_length = (int)strlen (label_text);
                                /*printf("labelin:%s\n",label_text); */

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
                                        /*printf("Label mag %d mag %d\n",label_mag,(scale_x*2)-1); */
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
//WE7U6
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

//printf("Found windows embedded symbol\n");

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
//WE7U6
                                // NOTE: 0x21 is the first color for the area object or "DOS" colors
                                draw_label_text (w, x+10, y+5, strlen(label_text),colors[0x21 + label_text_color],label_text);
                                symbol(w,0,label_symbol_del,label_symbol_char,' ',pixmap,1,x-10,y-10,' ');

                                if (debug_level & 512)
                                    printf("Windows map, embedded object: %c %c %c %s\n",
                                        label_symbol_del,label_symbol_char,label_text_color,label_text);
                            }
                            else {
                                if (debug_level & 512)
                                    printf("Weird label in Windows map, neither a plain label nor an object: %d %d\n",
                                        label_type_1[0],label_type_2[0]);
                            }
                        }
                    }
                }       // if (map_labels)
            }
            (void)fclose (f);
        }
        else
            printf("Couldn't open file: %s\n", file);
    }

    // Couldn't figure out the map type
    else {
        printf("draw_map: Unknown map type: %s\n", filenm);
    }

    XmUpdateDisplay (XtParent (da));
}  // End of draw_map()





/////////////////////////////////////////////////////////////////////
// map_search()
//
// Function which recurses through map directories, finding
// map files.  It's called from load_auto_maps and
// load_alert_maps.  If a map file is found, it is drawn.
// If alert == NULL, we looking for a regular map file to draw.
// If alert != NULL, we have a weather alert to draw.
//
// For alert maps, we need to do things a bit differently, as there
// should be only a few maps that contain all of the alert maps, and we
// can compute which map some of them might be in.  We need to fill in
// the alert structure with the filename that alert is found in.
/////////////////////////////////////////////////////////////////////
void map_search (Widget w, char *dir, alert_entry * alert, int *alert_count,int warn, int destination_pixmap) {
    struct dirent *dl = NULL;
    DIR *dm;
    char fullpath[8000];
    struct stat nfile;
    const time_t *ftime;
    char this_time[40];
    char *ptr;
    char *map_dir;
    int map_dir_length;

    // We'll use the weather alert directory if it's an alert
    map_dir = alert ? ALERT_MAP_DIR : WIN_MAP_DIR;

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
                    //printf("%c:County file\n",alert->title[3]);
                    strncpy (alert->filename, "c_", sizeof (alert->filename));
                    break;
                case 'A':   // 'A' in 4th char means county warning area
                    // Use County warning area w_?????
                    //printf("%c:County warning area file\n",alert->title[3]);
                    strncpy (alert->filename, "w_", sizeof (alert->filename));
                    break;
                case 'Z':
                    // Zone, coastal or offshore marine zone file z_????? or mz?????? or oz??????
                    // oz: ANZ081-086,088,PZZ081-085
                    // mz: AM,AN,GM,LC,LE,LH,LM,LO,LS,PH,PK,PM,PS,PZ,SL
                    // z_: All others
                    if (strncasecmp(alert->title,"AM",2) == 0) {
                        //printf("%c:Coastal marine zone file\n",alert->title[3]);
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
                            //printf("%c:Offshore marine zone file\n",alert->title[3]);
                            strncpy (alert->filename, "oz", sizeof (alert->filename));
                        }
                        else {
                            //printf("%c:Coastal marine zone file\n",alert->title[3]);
                            strncpy (alert->filename, "mz", sizeof (alert->filename));
                        }
                    }
                    else if (strncasecmp(alert->title,"GM",2) == 0) {
                        //printf("%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LC",2) == 0) {
                        //printf("%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LE",2) == 0) {
                        //printf("%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LH",2) == 0) {
                        //printf("%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LM",2) == 0) {
                        //printf("%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LO",2) == 0) {
                        //printf("%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LS",2) == 0) {
                        //printf("%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"PH",2) == 0) {
                        //printf("%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"PK",2) == 0) {
                        //printf("%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"PM",2) == 0) {
                        //printf("%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"PS",2) == 0) {
                        //printf("%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"PZ",2) == 0) {
// Need to check for PZZ081-085, if so use oz??????, else use mz??????
                        if (       (strncasecmp(&alert->title[3],"Z081",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z082",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z083",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z084",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z085",4) == 0) ) {
                            //printf("%c:Offshore marine zone file\n",alert->title[3]);
                            strncpy (alert->filename, "oz", sizeof (alert->filename));
                        }
                        else {
                            //printf("%c:Coastal marine zone file\n",alert->title[3]);
                            strncpy (alert->filename, "mz", sizeof (alert->filename));
                        }
                    }
                    else if (strncasecmp(alert->title,"SL",2) == 0) {
                        //printf("%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else {
                        // Must be regular zone file instead of coastal
                        // marine zone or offshore marine zone.
                        //printf("%c:Zone file\n",alert->title[3]);
                        strncpy (alert->filename, "z_", sizeof (alert->filename));
                    }
                    break;
                default:
                    // Unknown type
//printf("%c:Can't match weather warning to a Shapefile:%s\n",alert->title[3],alert->title);
                    break;
            }
//            printf("%s\t%s\t%s\n",alert->activity,alert->alert_status,alert->title);
            //printf("File: %s\n",alert->filename);
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
                    if (warn)
                        perror (fullpath);
                }
                else {    // We could open the directory just fine
                    while ( (dl = readdir(dm)) && !done ) {
                        xastir_snprintf(fullpath, sizeof(fullpath), "%s%s", dir, dl->d_name);
                        /*printf("FULL PATH %s\n",fullpath); */
                        if (stat (fullpath, &nfile) == 0) {
                            ftime = (time_t *)&nfile.st_ctime;
                            switch (nfile.st_mode & S_IFMT) {
                                case (S_IFDIR):     // It's a directory, skip it
                                    break;

                                case (S_IFREG):     // It's a file, check it
                                    /*printf("FILE %s\n",dl->d_name); */
                                    // Here we look for a match for the
                                    // first 2 characters of the filename.
                                    // 
                                    if (strncasecmp(alert->filename,dl->d_name,2) == 0) {
                                        // We have a match
                                        //printf("%s\n",fullpath);
                                        // Force last three characters to
                                        // "shp"
                                        dl->d_name[strlen(dl->d_name)-3] = 's';
                                        dl->d_name[strlen(dl->d_name)-2] = 'h';
                                        dl->d_name[strlen(dl->d_name)-1] = 'p';

                                        // Save the filename in the alert
                                        strncpy(alert->filename,dl->d_name,strlen(dl->d_name));
                                        done++;
                                        //printf("%s\n",dl->d_name);
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
                draw_map (w,
                    dir,                // Alert directory
                    alert->filename,    // Shapefile filename
                    alert,
                    -1,                 // Signifies "DON'T DRAW THE SHAPE"
                    destination_pixmap);
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
    else {  // We're doing regular maps, not weather alerts
        dm = opendir (dir);
        if (!dm) {  // Couldn't open directory
            xastir_snprintf(fullpath, sizeof(fullpath), "aprsmap %s", dir);
            if (warn)
                perror (fullpath);
        }
        else {
            int count = 0;
            while ((dl = readdir (dm))) {
                xastir_snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, dl->d_name);
                /*printf("FULL PATH %s\n",fullpath); */
                if (stat (fullpath, &nfile) == 0) {
                    ftime = (time_t *)&nfile.st_ctime;
                    switch (nfile.st_mode & S_IFMT) {
                        case (S_IFDIR):     // It's a directory, recurse
                            /*printf("file %c letter %c\n",dl->d_name[0],letter); */
                            if ((strcmp (dl->d_name, ".") != 0) && (strcmp (dl->d_name, "..") != 0)) {
                                strcpy (this_time, ctime (ftime));
                                map_search(w, fullpath, alert, alert_count, warn, destination_pixmap);
                            }
                            break;

                        case (S_IFREG):     // It's a file, draw the map
                            /*printf("FILE %s\n",dl->d_name); */

                            // Check whether the file is in a subdirectory
                            if (strncmp (fullpath, map_dir, (size_t)map_dir_length) != 0) {
                                draw_map (w,
                                    dir,
                                    dl->d_name,
                                    alert ? &alert[*alert_count] : NULL,
                                    '\0',
                                    destination_pixmap);
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
                                    destination_pixmap);
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
                printf ("Number of maps queried: %d\n", count);

            (void)closedir (dm);
        }
    }
}





/* moved these here and made them static so it will function on FREEBSD */
#define MAX_ALERT 7000
static alert_entry alert[MAX_ALERT];
static int alert_count;





/**********************************************************
 * load_alert_maps()
 *
 * Used to load weather alert maps, based on NWS weather
 * alerts that are received.  Called from create_image()
 * and refresh_image().  This version of the function is
 * designed to use ESRI Shapefile map files.
 * The base directory where the Shapefiles are located is
 * passed to us in the "dir" variable.
 **********************************************************/
// draw_map() fills in these fields in the alert structure:
//   alert->filename
//   alert->title
//   alert->top_boundary
//   alert->bottom_boundary
//   alert->left_boundary
//   alert->right_boundary
//   alert->flags[0];
//
// And here is what the structure looks like (defined in alert.h):
//typedef struct {
//    unsigned long top_boundary, left_boundary, bottom_boundary, right_boundary;
//    time_t expiration;
//    char activity[21];
//    char alert_tag[21];
//    char title[33];
//    char alert_level;
//    char from[10];
//    char to[10];
//    /* referenced flags
//       0 - on screen
//       1 - source
//    */
//    char flags[16];
//    char filename[64];
//} alert_entry;
//
void load_alert_maps (Widget w, char *dir) {
    int i, level;
    char alert_scan[400], *dir_ptr;

    /* gray86, red2, yellow2, cyan2, RoyalBlue, ForestGreen, orange3 */
    unsigned char fill_color[] = {  (unsigned char)0x69,
                                    (unsigned char)0x4a,
                                    (unsigned char)0x63,
                                    (unsigned char)0x66,
                                    (unsigned char)0x61,
                                    (unsigned char)0x64,
                                    (unsigned char)0x62 };

    alert_count = MAX_ALERT - 1;

    // Check for message alerts, draw alerts if they haven't expired yet
    // and they're in our view.
    if (alert_message_scan ()) {    // Returns number of wx alerts * 3
        memset (alert_scan, 0, sizeof (alert_scan));    // Zero our string
        strncpy (alert_scan, dir, 390); // Fetch the base directory
        strcat (alert_scan, "/");   // Complete alert directory is now set up in the string
        dir_ptr = &alert_scan[strlen (alert_scan)]; // Point to end of path

        //printf("Weather Alerts, alert_scan: %s\t\talert_status: %s\n", alert_scan, alert_status);

        // Iterate through the weather alerts we currently have.
        for (i = 0; i < alert_list_count; i++) {
            // The last parameter denotes loading into pixmap_alerts instead
            // of pixmap or pixmap_final.  Note that just calling map_search
            // gets the alert areas drawn on the screen via the draw_map()
            // function.
            //printf("load_alert_maps() Title: %s\n",alert_list[i].title);

// It looks like we want to do this section just to fill in the
// alert struct and to determine whether the alert is within our
// viewport.  We don't really wish to draw the alerts at this stage.
// That comes just a bit later in this routine.
            map_search (w,
                alert_scan,
                &alert_list[i],
                &alert_count,
                (int)(alert_status[i + 2] == DATA_VIA_TNC || alert_status[i + 2] == DATA_VIA_LOCAL),
                DRAW_TO_PIXMAP_ALERTS);
//printf("Title1:%s\n",alert_list[i].title);
        }
    }

// Just for a test
//draw_shapefile_map (w, dir, filenm, alert, alert_color, destination_pixmap);
//draw_shapefile_map (w, dir, "c_16my01.shp", NULL, '\0', DRAW_TO_PIXMAP_ALERTS);

    if (!alert_count)
        XtAppWarning (app_context, "Alert Map count overflow: load_alert_maps\b\n");

    for (i = MAX_ALERT - 1; i > alert_count; i--) {
        if (alert[i].flags[0] == 'Y' || alert[i].flags[0] == 'N')
            alert_update_list (&alert[i], ALERT_TITLE);
    }

    // Draw each alert map in the alert_list for which we have a
    // filename.
    for (i = 0; i < alert_list_count; i++) {

//printf("Title2:%s\n",alert_list[i].title);

        if (alert_list[i].filename[0]) {    // If filename is non-zero
            alert[0] = alert_list[i];       // Reordering the alert_list???

            // The last parameter denotes drawing into pixmap_alerts
            // instead of pixmap or pixmap_final.
// Why do we need to draw alerts again here?
//            draw_map (w,
//                dir,
//                alert_list[i].filename,
//                &alert[0],
//                '\0',
//                DRAW_TO_PIXMAP_ALERTS);

            alert_update_list (&alert[0], ALERT_ALL);
//printf("Title3:%s\n",alert_list[i].title);
        }
    }

    //printf("Calling alert_sort_active()\n");

    // Mark all of the active alerts in the list
    alert_sort_active ();

    //printf("Drawing all active alerts\n");

    // Run through all the alerts, drawing any that are active

// Are we drawing them in reverse order so that the important 
// alerts end up drawn on top of the less important alerts?

    for (i = alert_list_count - 1; i >= 0; i--) {
//        if (alert_list[i].flags[0] == 'Y' && (level = alert_active (&alert_list[i], ALERT_ALL))) {
        if ( (level = alert_active(&alert_list[i], ALERT_ALL) ) ) {
            if (level >= (int)sizeof (fill_color))
                level = 0;

            // The last parameter denotes drawing into pixmap_alert
            // instead of pixmap or pixmap_final.

// Why do we need to draw alerts again here?  Looks like it's to get
// the right tint color.

            //printf("Drawing %s\n",alert_list[i].filename);
            //printf("Title4:%s\n",alert_list[i].title);


            if (alert_list[i].alert_level != 'C') {
                draw_map (w,
                    dir,
                    alert_list[i].filename,
                    &alert_list[i],
                    fill_color[level],
                    DRAW_TO_PIXMAP_ALERTS);
            }
            else {
                // Cancelled alert, don't draw it!
            }
        }
    }


//for (i = 0; i < alert_list_count; i++)
//    printf("Title5:%s\n",alert_list[i].title);


    //printf("Done drawing all active alerts\n");

    if (alert_display_request()) {
        alert_redraw_on_update = redraw_on_new_data = 2;
    }
}





/**********************************************************
 * load_auto_maps()
 *
 * Recurses through the map directories looking for maps
 * to load.
 **********************************************************/
void load_auto_maps (Widget w, char *dir) {
    map_search (w, dir, NULL, NULL, (int)TRUE, DRAW_TO_PIXMAP);
}





/**********************************************************
 * load_maps()
 *
 * Loads maps, draws grid, updates the display.
 **********************************************************/
void load_maps (Widget w) {
    FILE *f;
    char mapname[300];
    int i;


    if (debug_level & 1)
        printf ("Load maps start\n");

    (void)filecreate(WIN_MAP_DATA);   // Create empty file if it doesn't exist
    f = fopen (WIN_MAP_DATA, "r");
    if (f != NULL) {
        if (debug_level & 1)
            printf ("Load maps Open map file\n");

        while (!feof (f)) {
            // Grab one line from the file
            if ( fgets( mapname, 299, f ) != NULL ) {

                // Get rid of the newline at the end
                for (i = strlen(mapname); i > 0; i--) {
                    if (mapname[i] == '\n')
                        mapname[i] = '\0'; 
                }

                if (debug_level & 1)
                    printf("Found mapname: %s\n", mapname);

                if (mapname[0] != '#') {
                    draw_map (w, WIN_MAP_DIR, mapname, NULL, '\0', DRAW_TO_PIXMAP);

                    if (debug_level & 1)
                        printf ("Load maps -%s\n", mapname);

                    XmUpdateDisplay (da);
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
        printf("Couldn't open file: %s\n", WIN_MAP_DATA);

    if (debug_level & 1)
        printf ("Load maps stop\n");
}


