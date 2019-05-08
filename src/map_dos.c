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
#include "util.h"
#include "main.h"
#include "datum.h"
#include "draw_symbols.h"
#include "rotated.h"
#include "color.h"
#include "xa_config.h"

// Must be last include file
#include "leak_detection.h"



#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }

#define DOS_HDR_LINES 8
#define GRID_MORE 5000

extern int npoints;        /* Global defined in maps.c: "number of points in a line" */
extern int mag;

/* MAP pointers */

//static map_vectors *map_vectors_ptr;
//static text_label *map_text_label_ptr;
//static symbol_label *map_symbol_label_ptr;

/* MAP counters */

//static long vectors_num;
//static long text_label_num;
//static long object_label_num;




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

    }   // end of switch
    else {    // Display all colors
        draw_ok = 1;
    }

    if (draw_ok) {
        x = ((x_long_cord - NW_corner_longitude) / scale_x);
        y = ((y_lat_cord  - NW_corner_latitude)  / scale_y);
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
                    }
                    else {
                        fill_color = (unsigned char)object_behavior;
                    }


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
//                                    fprintf(stderr,
//                                        "map_plot:Too few points:%d, Skipping XFillPolygon()",
//                                        npoints);
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
//                                fprintf(stderr,
//                                    "map_plot:Too few points:%d, Skipping XFillPolygon()",
//                                    npoints);
                            }
                            break;

                    }   // end of switch

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
                        (void)XDrawLines (XtDisplay (w),
                            pixmap_final,
                            gc,
                            points,
                            l16(npoints),
                            CoordModeOrigin);
                        break;

                    case DRAW_TO_PIXMAP:
                        (void)XDrawLines (XtDisplay (w),
                            pixmap,
                            gc,
                            points,
                            l16(npoints),
                            CoordModeOrigin);
                        break;

                    case DRAW_TO_PIXMAP_ALERTS:
                        fprintf(stderr,"You're calling the wrong routine to draw weather alerts!\n");
                        break;
                }   // end of switch

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
            points[npoints].x = l16(x);
            points[npoints].y = l16(y);
            if (    (points[npoints].x > (-MAX_OUTBOUND))
                 && (points[npoints].x < (short)max_x)
                 && (points[npoints].y > (-MAX_OUTBOUND))
                 && (points[npoints].y < (short)max_y)
                 && (color != (unsigned char)0) )

                npoints++;

            last_behavior = (unsigned char)object_behavior;
            return;
        }
        points[npoints].x = l16(x);
        points[npoints].y = l16(y);
        last_behavior = (unsigned char)object_behavior;

        if (npoints == 0) { // First point drawn in the line
            if (points[npoints].x > (-MAX_OUTBOUND)
                    && points[npoints].x < (short)max_x
                    && points[npoints].y > (-MAX_OUTBOUND)
                    && points[npoints].y < (short)max_y) {
                npoints++;
            }
        }
        else { 
            if (points[npoints].x != points[npoints - 1].x || points[npoints].y != points[npoints - 1].y) {
                if (last_behavior & 0x80) {
                    npoints++;
                }
                else if (points[npoints].x > (-MAX_OUTBOUND)
                         && points[npoints].x < (short)max_x
                         && points[npoints].y > (-MAX_OUTBOUND)
                         && points[npoints].y < (short)max_y) {
                    npoints++;
                }
            }
        }
    }
    else {
        npoints = 0;
    }
}   /* map_plot */





void draw_dos_map(Widget w,
                  char *dir,
                  char *filenm,
                  alert_entry *alert,
                  u_char alert_color,
                  int destination_pixmap,
                  map_draw_flags *mdf) {  

    FILE *f;
    char file[MAX_FILENAME];
    char short_filenm[MAX_FILENAME];
    char map_it[MAX_FILENAME];

    /* map header info */
    char map_type[5];
    char map_version[5];
    char file_name[33];
//    char *ext;
    char map_title[33];
    char map_creator[9];
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
//  int map_range;
  
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
//  int map_maxed_vectors;
//  int map_maxed_text_labels;
//  int map_maxed_symbol_labels;
//  map_vectors *vectors_ptr;
//  text_label *text_ptr;
//  symbol_label *symbol_ptr;
    int line_width;
    int x, y;
    int color;
    long max_x, max_y;
    int in_window = 0;
    char symbol_table;
    char symbol_id;
    char symbol_color;
    int embedded_object;

    int draw_filled;
    unsigned char last_behavior, special_fill = (unsigned char)FALSE;


    draw_filled=mdf->draw_filled;

    x = 0;
    y = 0;
    color = -1;
    line_width = 1;
    mag = (1 * scale_y) / 2;    // determines if details are drawn
  
    /* MAP counters */
//  vectors_ptr = map_vectors_ptr;
//  text_ptr = map_text_label_ptr;
//  symbol_ptr = map_symbol_label_ptr;
  
//  map_maxed_vectors = 0;
//  map_maxed_text_labels = 0;
//  map_maxed_symbol_labels = 0;
    npoints = 0;
  
    xastir_snprintf(file, sizeof(file), "%s/%s", dir, filenm);

    // Create a shorter filename for display (one that fits the
    // status line more closely).  Subtract the length of the
    // "Indexing " and/or "Loading " strings as well.
    if (strlen(filenm) > (41 - 9)) {
        int avail = 41 - 11;
        int new_len = strlen(filenm) - avail;

        xastir_snprintf(short_filenm,
            sizeof(short_filenm),
            "..%s",
            &filenm[new_len]);
    }
    else {
        xastir_snprintf(short_filenm,
            sizeof(short_filenm),
            "%s",
            filenm);
    }

    f = fopen (file, "r");
    if (f == NULL) {
        fprintf(stderr,"Couldn't open file: %s\n", file);
        return;
    }

    if (fread (map_type, 4, 1, f) == 0) {
        // Ignoring fread errors as we've done here since forever.
        // Would be good to take another look at this later.
    }
 
    map_type[4] = '\0';
    dos_labels = FALSE;
    points_per_degree = 300;
    

// DOS-type map header portion of the code.

    if (strtod (map_type, &ptr) > 0.01 && (*ptr == '\0' || *ptr == ' ' || *ptr == ',')) {
        int j;
      
        if (debug_level & 512)
            fprintf(stderr,"\nDOS Map\n");
      
        top_boundary = left_boundary = bottom_boundary = right_boundary = 0;
        rewind (f);
        map_title[0] = map_creator[0] = Buffer[0] = '\0';
        // set map_type for DOS ASCII maps
        xastir_snprintf(map_type,sizeof(map_type),"DOS ");
        map_type[4] = '\0';
        xastir_snprintf(file_name,sizeof(file_name),"%s",filenm);
        total_vector_points = 200000;
        total_labels = 2000;

        for (j = 0; j < DOS_HDR_LINES; strlen(Buffer) ? 1 : j++) {

            if (fgets (&Buffer[strlen (Buffer)],(int)sizeof (Buffer) - (strlen (Buffer)), f) == 0) {
                // Ignoring fgets errors as we've done here since forever.
                // Would be good to take another look at this later.
            }
 

//            if (!strlen(Buffer))
//                j++;
 
            while ((ptr = strpbrk (Buffer, "\r\n")) != NULL && j < DOS_HDR_LINES) {

                *ptr = '\0';

                for (ptr++; *ptr == '\r' || *ptr == '\n'; ptr++) ;

                switch (j) {

                    case 0:
//fprintf(stderr,"top_boundary: %s\n", Buffer);
                        top_boundary = (unsigned long) (-atof (Buffer) * 360000 + 32400000);
                        break;

                    case 1:
//fprintf(stderr,"left_boundary: %s\n", Buffer);
                        left_boundary = (unsigned long) (-atof (Buffer) * 360000 + 64800000);
                        break;

                    case 2:
//fprintf(stderr,"points_per_degree: %s\n", Buffer);
                        points_per_degree = (int) atof (Buffer);
                        break;

                    case 3:
//fprintf(stderr,"bottom_boundary: %s\n", Buffer);
                        bottom_boundary = (unsigned long) (-atof (Buffer) * 360000 + 32400000);
                        bottom_boundary = bottom_boundary + bottom_boundary - top_boundary;
                        break;

                    case 4:
//fprintf(stderr,"right_boundary: %s\n", Buffer);
                        right_boundary = (unsigned long) (-atof (Buffer) * 360000 + 64800000);
                        right_boundary = right_boundary + right_boundary - left_boundary;
                        break;

                    case 5:
//fprintf(stderr,"map_range: %s\n", Buffer);
//                        map_range = (int) atof (Buffer);
                        break;

                    case 7:
//fprintf(stderr,"Map Version: %s\n", Buffer);
                        xastir_snprintf(map_version,sizeof(map_version),"%s",Buffer);
//fprintf(stderr,"MAP VERSION: %s\n", map_version);
                        break;
                } // end of switch

                xastir_snprintf(Buffer,sizeof(Buffer),"%s",ptr);

//                if (strlen (Buffer))
//                    j++;

            }
        }   // End of DOS-type map header portion
    }
    else {

// Windows-type map header portion

        if (debug_level & 512)
            fprintf(stderr,"\nWindows map\n");

        if (fread (map_version, 4, 1, f) == 0) {
            // Ignoring fread errors as we've done here since forever.
            // Would be good to take another look at this later.
        }
 
        map_version[4] = '\0';
      
        if (fread (file_name, 32, 1, f) == 0) {
            // Ignoring fread errors as we've done here since forever.
            // Would be good to take another look at this later.
        }
 
        file_name[32] = '\0';
      
        if (fread (map_title, 32, 1, f) == 0) {
            // Ignoring fread errors as we've done here since forever.
            // Would be good to take another look at this later.
        }
 
        map_title[32] = '\0';
        if (debug_level & 16)
            fprintf(stderr,"Map Title %s\n", map_title);
      
        if (fread (map_creator, 8, 1, f) == 0) {
            // Ignoring fread errors as we've done here since forever.
            // Would be good to take another look at this later.
        }
 
        map_creator[8] = '\0';
        if (debug_level & 16)
            fprintf(stderr,"Map Creator %s\n", map_creator);
      
        if (fread (&temp, 4, 1, f) == 0) {
            // Ignoring fread errors as we've done here since forever.
            // Would be good to take another look at this later.
        }
 
        creation_date = ntohl (temp);
        if (debug_level & 16)
            fprintf(stderr,"Creation Date %lX\n", creation_date);
      
        year = creation_date / 31536000l;
        days = (creation_date - (year * 31536000l)) / 86400l;
        if (debug_level & 16)
            fprintf(stderr,"year is %ld + days %ld\n", 1904l + year, (long)days);
      
        if (fread (&temp, 4, 1, f) == 0) {
            // Ignoring fread errors as we've done here since forever.
            // Would be good to take another look at this later.
        }
 
        left_boundary = ntohl (temp);
      
        if (fread (&temp, 4, 1, f) == 0) {
            // Ignoring fread errors as we've done here since forever.
            // Would be good to take another look at this later.
        }
 
        right_boundary = ntohl (temp);
      
        if (fread (&temp, 4, 1, f) == 0) {
            // Ignoring fread errors as we've done here since forever.
            // Would be good to take another look at this later.
        }
 
        top_boundary = ntohl (temp);
      
        if (fread (&temp, 4, 1, f) == 0) {
            // Ignoring fread errors as we've done here since forever.
            // Would be good to take another look at this later.
        }
 
        bottom_boundary = ntohl (temp);
      
        if (strcmp (map_version, "2.00") != 0) {
            left_boundary *= 10;
            right_boundary *= 10;
            top_boundary *= 10;
            bottom_boundary *= 10;
        }

        if (fread (map_reserved1, 8, 1, f) == 0) {
            // Ignoring fread errors as we've done here since forever.
            // Would be good to take another look at this later.
        }
 
        if (fread (&temp, 4, 1, f) == 0) {
            // Ignoring fread errors as we've done here since forever.
            // Would be good to take another look at this later.
        }
 
        total_vector_points = (long)ntohl (temp);

        if (fread (&temp, 4, 1, f) == 0) {
            // Ignoring fread errors as we've done here since forever.
            // Would be good to take another look at this later.
        }
 
        total_labels = (long)ntohl (temp);

        if (fread (map_reserved2, 140, 1, f) == 0) {
            // Ignoring fread errors as we've done here since forever.
            // Would be good to take another look at this later.
        }
 
    }   // End of Windows-type map header portion
      
      
    // Done processing map header info.  The rest of this
    // function performs the actual drawing of both DOS-type
    // and Windows-type maps to the screen.
    
    
    if (debug_level & 16) {
        fprintf(stderr,"Map Type: %s, Version: %s, Filename: %s\n", map_type, map_version, file_name);
        fprintf(stderr,"Left Boundary: %ld, Right Boundary: %ld\n", (long)left_boundary,(long)right_boundary);
        fprintf(stderr,"Top Boundary: %ld, Bottom Boundary: %ld\n", (long)top_boundary,(long)bottom_boundary);
        fprintf(stderr,"Total vector points: %ld, total labels: %ld\n",total_vector_points, total_labels);
    }

 
    // Check whether we're indexing or drawing the map
    if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
            || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {
      
        // We're indexing only.  Save the extents in the index.
        index_update_xastir(filenm, // Filename only
                            bottom_boundary,  // Bottom
                            top_boundary,     // Top
                            left_boundary,    // Left
                            right_boundary,   // Right
                            1000);            // Default Map Level
      
        (void)fclose (f);

        // Update the statusline 
        xastir_snprintf(map_it,
                        sizeof(map_it),
                        langcode ("BBARSTA039"),
                        short_filenm);
        statusline(map_it,0);       // Loading/Indexing ...

        return; // Done indexing this file
    }
      
    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        (void)fclose(f);

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
  
    // Check to see if we should draw the map
    in_window = map_onscreen(left_boundary, right_boundary, top_boundary, bottom_boundary, 1);

    if (!in_window) {
        (void)fclose (f);
        return;
    }

    // Update the statusline
    xastir_snprintf(map_it,
        sizeof(map_it),
        langcode ("BBARSTA028"),
        short_filenm);
    statusline(map_it,0);       // Loading/Indexing ...

    object_behavior = '\0';

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

        HandlePendingEvents(app_context);
        if (interrupt_drawing_now) {
            (void)fclose(f);

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

// DOS type map
 
        if (strncmp ("DOS ", map_type, 4) == 0) {

            if (fgets (&Buffer[strlen (Buffer)],(int)sizeof (Buffer) - (strlen (Buffer)), f) == 0) {
                // Ignoring fgets errors as we've done here since forever.
                // Would be good to take another look at this later.
            }
 
            while ((ptr = strpbrk (Buffer, "\r\n")) != NULL && !dos_labels) {
                long LatHld = 0, LongHld;
                char *trailer;

                *ptr = '\0';

                for (ptr++; *ptr == '\r' || *ptr == '\n'; ptr++) ;
            
process:        if (strncasecmp ("Line", map_version, 4) == 0) {
                    int k;

                    color = (int)strtol (Buffer, &trailer, 0);

                    if (trailer && (*trailer == ',' || *trailer == ' ')) {
                        trailer++;

                        if (color == -1) {
                            dos_labels = (int)TRUE;
                            xastir_snprintf(Buffer,sizeof(Buffer),"%s",ptr);
                            break;
                        }

                        for (k = strlen (trailer) - 1; k >= 0; k--) {
                            trailer[k] = (char)( (int)trailer[k] - 27 );
                        }
                
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
                }

                else if (strncasecmp ("ASCII", map_version, 4) == 0) {

                    if (color == 0) {
                        color = (int)strtol (Buffer, &trailer, 0);
                        if (trailer && strpbrk (trailer, ", ")) {

                            for (; *trailer == ',' || *trailer == ' '; trailer++) ;

                            dos_flag = (int)strtol (trailer, &trailer, 0);
                            if (dos_flag == -1)
                            dos_labels = (int)TRUE;
                        }
                    }
                    else {
                        LongHld = strtol (Buffer, &trailer, 0);

                        if (trailer && strpbrk (trailer, ", ")) {

                            for (; *trailer == ',' || *trailer == ' '; trailer++) ;

                            LatHld = strtol (trailer, &trailer, 0);
                        }
                        else if (LongHld == 0 && *trailer != '\0') {
                            xastir_snprintf(map_version,sizeof(map_version),"Comp");
                            map_version[4] = '\0';
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
                        }
                        else if (LongHld == 0 && LatHld == -1) {
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
                        }
                        else {
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
                else if (strncasecmp ("Comp", map_version, 4) == 0) {
                    char Tag[81];
                    int k;

                    Tag[80] = '\0';
                    if (color == 0) {
                        color = (int)strtol (Buffer, &trailer, 0);
                        if (trailer && strpbrk (trailer, ", ")) {

                            for (; *trailer == ',' || *trailer == ' '; trailer++) ;

                            dos_flag = (int)strtol (trailer, &trailer, 0);
                            xastir_snprintf(Tag,sizeof(Tag),"%s",trailer);
                            Tag[79] = '\0';
                            if (dos_flag == -1)
                            dos_labels = (int)TRUE;
                        }
                    }
                    else {
                        LongHld = strtol (Buffer, &trailer, 0);

                        for (; *trailer == ',' || *trailer == ' '; trailer++) ;

                        LatHld = strtol (trailer, &trailer, 0);
                        if (LatHld == 0 && *trailer != '\0') {
                            LatHld = 1;
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
                        }
                        else if (LongHld == 0 && LatHld == -1) {
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

                            for (k = strlen (trailer) - 1; k >= 0; k--) {
                                trailer[k] = (char)((int)trailer[k] - 27);
                            }
                  
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
                }
                else {
                    LongHld = strtol (Buffer, &trailer, 0);
                    if (trailer) {
                        if (*trailer == ',' || *trailer == ' ') {
                            if (LongHld == 0) {
                                xastir_snprintf(map_version,sizeof(map_version),"ASCII");
                            }
    
                            map_version[4] = '\0';
                  
                            trailer++;

                            dos_flag = (int)strtol (trailer, &trailer, 0);
                            if (dos_flag == -1) {
                                dos_labels = (int)TRUE;
                            }
                  
                            if (dos_flag == 0 && *trailer != '\0') {
                                xastir_snprintf(map_version,sizeof(map_version),"Line");
                                map_version[4] = '\0';
                                goto process;
                            }
                            color = (int)LongHld;
                        }
                    }
                    else {
                        xastir_snprintf(map_version,sizeof(map_version),"Comp");
                   }
                    map_version[4] = '\0';
                }
                xastir_snprintf(Buffer,sizeof(Buffer),"%s",ptr);
            }
        }
        else {

// Windows type map...

            last_behavior = object_behavior;

            if (fread (&vector_start, 1, 1, f) == 0) {
                // Ignoring fread errors as we've done here since forever.
                // Would be good to take another look at this later.
            }
 
            if (fread (&object_behavior, 1, 1, f) == 0) {      // Fill Color?
                // Ignoring fread errors as we've done here since forever.
                // Would be good to take another look at this later.
            }
 
            if (strcmp (map_type, "COMP") == 0) {
                short temp_short;
                long LatOffset, LongOffset;

                LatOffset  = (long)(top_boundary  - top_boundary  % 6000);
                LongOffset = (long)(left_boundary - left_boundary % 6000);

                if (fread (&temp_short, 2, 1, f) == 0) {
                    // Ignoring fread errors as we've done here since forever.
                    // Would be good to take another look at this later.
                }

                x_long_cord = (ntohs (temp_short) * 10 + LongOffset);

                if (fread (&temp_short, 2, 1, f) == 0) {
                    // Ignoring fread errors as we've done here since forever.
                    // Would be good to take another look at this later.
                }
 
                y_lat_cord  = (ntohs (temp_short) * 10 + LatOffset);
            }
            else {
                if (fread (&temp, 4, 1, f) == 0) {
                    // Ignoring fread errors as we've done here since forever.
                    // Would be good to take another look at this later.
                }

                x_long_cord = ntohl (temp);

                if (strcmp (map_version, "2.00") != 0)
                    x_long_cord *= 10;
            
                if (fread (&temp, 4, 1, f) == 0) {
                    // Ignoring fread errors as we've done here since forever.
                    // Would be good to take another look at this later.
                }
 
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
    if (alert_color) {
        map_plot (w,
            max_x,
            max_y,
            0,
            0,
            '\0',
            special_fill ? (long)0xfd : (long)alert_color,
            destination_pixmap,
            draw_filled);
    }
    else {
        map_plot (w,
            max_x,
            max_y,
            0,
            0,
            (unsigned char)0xff,
            0,
            destination_pixmap,
            draw_filled);
    }
      
    (void)XSetForeground (XtDisplay (w), gc, colors[20]);
    line_width = 2;
    (void)XSetLineAttributes (XtDisplay (w), gc, line_width, LineSolid, CapButt,JoinMiter);

      
    // Here is the map label section of the code for both DOS & Windows-type maps
    if (map_labels) {

        HandlePendingEvents(app_context);
        if (interrupt_drawing_now) {
            (void)fclose(f);

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

        /* read labels */
        for (count = 0l; count < total_labels && !feof (f); count++) {

//DOS-Type Map Labels

            embedded_object = 0;

            if (strcmp (map_type, "DOS ") == 0) {   // Handle DOS-type map labels/embedded objects
                char *trailer;

                if (fgets (&Buffer[strlen (Buffer)],(int)sizeof (Buffer) - (strlen (Buffer)), f) == 0) {
                    // Ignoring fgets errors as we've done here since forever.
                    // Would be good to take another look at this later.
                }
 
                for (; (ptr = strpbrk (Buffer, "\r\n")) != NULL;xastir_snprintf(Buffer,sizeof(Buffer),"%s",ptr)) {

                    *ptr = '\0';
                    label_type[0] = (char)0x08;

                    for (ptr++; *ptr == '\r' || *ptr == '\n'; ptr++) ;

                    trailer = strchr (Buffer, ',');
                    if (trailer && strncmp (Buffer, "0", 1) != 0) {
                        *trailer = '\0';
                        trailer++;
                        xastir_snprintf(label_text,sizeof(label_text),"%s",Buffer);
                
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
                                // Take the object out of the label text
                                xastir_snprintf(label_text,sizeof(label_text),"%s",Buffer+3);
                            }
                            else {  // Could be in new or old format with a leading '#' character
                                symbol_table = label_text[1];
                                if (symbol_table == '/' || symbol_table == '\\') {  // New format: #/xC
                                    symbol_id = label_text[2];
                                    symbol_color = label_text[3];
                                    // Take the object out of the label text
                                    xastir_snprintf(label_text,sizeof(label_text),"%s",Buffer+4);
                                }
                                else {                                  // Old format: #xC
                                    symbol_table = '\\';
                                    symbol_id = label_text[1];
                                    symbol_color = label_text[2];
                                    // Take the object out of the label text
                                    xastir_snprintf(label_text,sizeof(label_text),"%s",Buffer+3);
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
                            x = ((label_x_cord - NW_corner_longitude) / scale_x) - (label_length * 6);
                        else  /* right of coords */
                            x = ((label_x_cord - NW_corner_longitude) / scale_x);
                
                        y = ((label_y_cord - NW_corner_latitude) / scale_y);
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
            }
            else {    // Handle Windows-type map labels/embedded objects
                int rotation = 0;
                char rotation_factor[5];
            
// Windows-Type Map Labels

                char label_type_1[2], label_type_2[2];
            
                // Snag first two bytes of label
                if (fread (label_type_1, 1, 1, f) == 0) {
                    // Ignoring fread errors as we've done here since forever.
                    // Would be good to take another look at this later.
                }
 
                if (fread (label_type_2, 1, 1, f) == 0) {
                    // Ignoring fread errors as we've done here since forever.
                    // Would be good to take another look at this later.
                }
            
                if (label_type_2[0] == '\0') {  // Found a label
              
                    // Found text label
                    if (fread (&temp, 4, 1, f) == 0) {           /* x */
                        // Ignoring fread errors as we've done here since forever.
                        // Would be good to take another look at this later.
                    }
 
                    label_x_cord = ntohl (temp);
                    if (strcmp (map_version, "2.00") != 0)
                        label_x_cord *= 10;
              
                    if (fread (&temp, 4, 1, f) == 0) {           /* y */
                        // Ignoring fread errors as we've done here since forever.
                        // Would be good to take another look at this later.
                    }
 
                    label_y_cord = ntohl (temp);
                    if (strcmp (map_version, "2.00") != 0)
                        label_y_cord *= 10;
              
                    if (fread (&temp_mag, 2, 1, f) == 0) {       /* mag */
                        // Ignoring fread errors as we've done here since forever.
                        // Would be good to take another look at this later.
                    }

                    label_mag = (int)ntohs (temp_mag);
                    if (strcmp (map_version, "2.00") != 0)
                        label_mag *= 10;
              
                    if (strcmp (map_type, "COMP") == 0) {

                        for (i = 0; i < 32; i++) {
                            if (fread (&label_text[i], 1, 1, f) == 0) {
                                // Ignoring fread errors as we've done here since forever.
                                // Would be good to take another look at this later.
                            }
 
                            if (label_text[i] == '\0') {
                                break;
                            }
                        }
                        label_text[32] = '\0';  // Make sure we have a terminator
                    }
                    else {
                        if (fread (label_text, 32, 1, f) == 0) {   /* text */
                            // Ignoring fread errors as we've done here since forever.
                            // Would be good to take another look at this later.
                        }
 
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
                        for ( i=1; i<4; i++ ) {
                            rotation_factor[i-1] = label_text[i];
                        }

                        rotation_factor[3] = '\0';
                        rotation = atoi(rotation_factor);
                
                        // Take rotation factor out of label string
                        for ( i=4, j=0; i < (int)(strlen(label_text)+1); i++,j++) {
                            label_text[j] = label_text[i];
                        }
                
                        //fprintf(stderr,"Windows label: %s, rotation factor: %d\n",label_text, rotation);
                    }
              
                    label_length = (int)strlen (label_text);

                    for (i = (label_length - 1); i > 0; i--) {
                        if (label_text[i] == ' ') {
                            label_text[i] = '\0';
                        }
                        else {
                            break;
                        }
                    }
              
                    label_length = (int)strlen (label_text);
                    /*fprintf(stderr,"labelin:%s\n",label_text); */
              
                    if ((label_type_1[0] & 0x80) == '\0') {
                        /* left of coords */
                        x = ((label_x_cord - NW_corner_longitude) / scale_x) - (label_length * 6);
                        x = 0;  // ??????
                    }
                    else {
                        /* right of coords */
                        x = ((label_x_cord - NW_corner_longitude) / scale_x);
                    }
              
                    y = ((label_y_cord - NW_corner_latitude) / scale_y);
              
                    if (x > (0) && (x < (int)screen_width)) {

                        if (y > (0) && (y < (int)screen_height)) {

                            /*fprintf(stderr,"Label mag %d mag %d\n",label_mag,(scale_x*2)-1); */
                            //if (label_mag > (int)((scale_x * 2) - 1) || label_mag == 0)
                            if (label_mag > (int)((scale_x) - 1) || label_mag == 0) {
                                // Note: We're not drawing the labels in the right colors
                                if (rotation == 0) {    // Non-rotated label
//                                    draw_label_text (w,
//                                        x,
//                                        y,
//                                        label_length,
//                                        colors[(int)(label_type_1[0] & 0x7f)],
//                                        label_text);
                                    draw_rotated_label_text (w,
                                        -90.0,
                                        x,
                                        y,
                                        label_length,
                                        colors[(int)(label_type_1[0] & 0x7f)],
                                        label_text,
                                        FONT_DEFAULT);
                                }
                                else {  // Rotated label
                                    draw_rotated_label_text (w,
                                        rotation,
                                        x,
                                        y,
                                        label_length,
                                        colors[(int)(label_type_1[0] & 0x7f)],
                                        label_text,
                                        FONT_DEFAULT);
                                }
                            }
                        }
                    }
                }
                else if (label_type_2[0] == '\1' && label_type_1[0] == '\0') { // Found an embedded object
              
                    //fprintf(stderr,"Found windows embedded symbol\n");
              
                    /* label is an embedded symbol */
                    if (fread (&temp, 4, 1, f) == 0) {
                        // Ignoring fread errors as we've done here since forever.
                        // Would be good to take another look at this later.
                    }

                    label_x_cord = ntohl (temp);
                    if (strcmp (map_version, "2.00") != 0)
                        label_x_cord *= 10;
          
                    if (fread (&temp, 4, 1, f) == 0) {
                        // Ignoring fread errors as we've done here since forever.
                        // Would be good to take another look at this later.
                    }

                    label_y_cord = ntohl (temp);
                    if (strcmp (map_version, "2.00") != 0)
                        label_y_cord *= 10;
              
                    if (fread (&temp_mag, 2, 1, f) == 0) {
                        // Ignoring fread errors as we've done here since forever.
                        // Would be good to take another look at this later.
                    }
 
                    label_mag = (int)ntohs (temp_mag);
                    if (strcmp (map_version, "2.00") != 0)
                        label_mag *= 10;
              
                    if (fread (&label_symbol_del, 1, 1, f) == 0) { // Snag symbol table char
                        // Ignoring fread errors as we've done here since forever.
                        // Would be good to take another look at this later.
                    }

                    if (fread (&label_symbol_char, 1, 1, f) == 0) { // Snag symbol char
                        // Ignoring fread errors as we've done here since forever.
                        // Would be good to take another look at this later.
                    }
                    if (fread (&label_text_color, 1, 1, f) == 0) { // Snag text color (should be 1-9, others should default to black)
                        // Ignoring fread errors as we've done here since forever.
                        // Would be good to take another look at this later.
                    }

                    if (label_text_color < '1' && label_text_color > '9')
                        label_text_color = '0'; // Default to black
              
                    x = ((label_x_cord - NW_corner_longitude) / scale_x);
                    y = ((label_y_cord - NW_corner_latitude) / scale_y);
              
                    // Read the label text portion
                    if (strcmp (map_type, "COMP") == 0) {

                        for (i = 0; i < 32; i++) {
                            if (fread (&label_text[i], 1, 1, f) == 0) {
                                // Ignoring fread errors as we've done here since forever.
                                // Would be good to take another look at this later.
                            }

                            if (label_text[i] == '\0')
                                break;
                        }
                    }
                    else {
                        if (fread (label_text, 29, 1, f) == 0) {
                            // Ignoring fread errors as we've done here since forever.
                            // Would be good to take another look at this later.
            }
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

    (void)fclose (f);
}


