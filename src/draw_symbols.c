/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2002  The Xastir Group
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
#include <math.h>

#include <Xm/XmAll.h>

#include "db.h"
#include "draw_symbols.h"
#include "xastir.h"
#include "main.h"
#include "util.h"
#include "color.h"

#define ANGLE_UPDOWN 30         /* prefer horizontal cars if less than 45 degrees */

int symbols_loaded;
Widget select_symbol_dialog = (Widget)NULL;
static xastir_mutex select_symbol_dialog_lock;
Pixmap select_icons[(126-32)*2];    //33 to 126 with both '/' and '\' symbols (94 * 2) or 188
int symbol_change_requested_from = 0;


 


void draw_symbols_init(void)
{
    init_critical_section( &select_symbol_dialog_lock );
}





/*** symbol data ***/

void clear_symbol_data(void) {
    int my_size;
    int i;
    char *data_ptr;

    data_ptr = (char *)symbol_data;
    my_size = (int)sizeof(SymbolData);
    for (i=0;i<my_size;i++)
        *data_ptr++ = '\0';
    symbols_loaded = 0;
}





/* 
 *  Draw nice looking text
 */
void draw_nice_string(Widget w, Pixmap where, int style, long x, long y, char *text, int bgcolor, int fgcolor, int length) {

    switch (style) {
        case 0:
            // make outline style
            (void)XSetForeground(XtDisplay(w),gc,colors[bgcolor]);
            // draw an outline 1 pixel bigger than text
            (void)XDrawString(XtDisplay(w),where,gc,x+1,y-1,text,length);
            (void)XDrawString(XtDisplay(w),where,gc,x+1,y,text,length);
            (void)XDrawString(XtDisplay(w),where,gc,x+1,y+1,text,length);
            (void)XDrawString(XtDisplay(w),where,gc,x-1,y,text,length);
            (void)XDrawString(XtDisplay(w),where,gc,x-1,y-1,text,length);
            (void)XDrawString(XtDisplay(w),where,gc,x-1,y+1,text,length);
            (void)XDrawString(XtDisplay(w),where,gc,x,y+1,text,length);
            (void)XDrawString(XtDisplay(w),where,gc,x,y-1,text,length);
            break;
        case 1:
            // draw text the old way in a gray box
            // Leave this next one hard-coded to 0xff.  This keeps
            // the background as gray.
            (void)XSetForeground(XtDisplay(w),gc,colors[0xff]);
            (void)XFillRectangle(XtDisplay(w),where,gc,x-1,(y-10),(length*6)+2,11);
            (void)XSetForeground(XtDisplay(w),gc,colors[bgcolor]);
            (void)XDrawString(XtDisplay(w),where,gc,x+1,y+1,text,length);
            break;
        case 2:
        default:
            // draw white or colored text in a black box
            (void)XSetForeground(XtDisplay(w),gc,(int)GetPixelByName(w,"black") );
            (void)XFillRectangle(XtDisplay(w),where,gc,x-2,(y-11),(length*6)+3,13);
            break;

    }

    // finally draw the text
    (void)XSetForeground(XtDisplay(w),gc,colors[fgcolor]);
    (void)XDrawString(XtDisplay(w),where,gc,x,y,text,length);
}





/* symbol drawing routines */




//draw_pod_circle(64000000l, 32400000l, 10, colors[0x44], pixmap_final);
//
// Probability of Detection circle:  A circle around the point last
// seen drawn at the distance that a person of that description
// could travel since they were last seen.  It helps to limit a
// search to a reasonable area.
//
// It'd be nice to have some method of showing where the center of
// the circle was as well, in case we don't have a PLS object placed
// there also.  Perhaps a small dot and/or four lines going from
// that point to the edge of the circle?
//
// range is in miles
// x_long/y_lat are in Xastir lat/lon units
//
void draw_pod_circle(long x_long, long y_lat, double range, int color, Pixmap where) {
    double diameter;
    long max_x, max_y;
    double a,b;


    /* max off screen values */
    max_x = screen_width+800l;
    max_y = screen_height+800l;
    if ((x_long>=0) && (x_long<=129600000l)) {
        if ((y_lat>=0) && (y_lat<=64800000l)) {

// Prevents it from being drawn when the symbol is off-screen.
// It'd be better to check for lat/long +/- range to see if it's on the screen.

            if ((x_long>x_long_offset) && (x_long<(x_long_offset+(long)(screen_width *scale_x)))) {
                if ((y_lat>y_lat_offset) && (y_lat<(y_lat_offset+(long)(screen_height*scale_y)))) {

                    // Range is in miles.  Bottom term is in meters before the 0.0006214
                    // multiplication factor which converts it to miles.
                    // Equation is:  2 * ( range(mi) / x-distance across window(mi) )
                    diameter = 2.0 * ( range/
                        (scale_x * calc_dscale_x(mid_x_long_offset,mid_y_lat_offset) * 0.0006214 ) );

                    a=diameter;
                    b=diameter/2;

                    //printf("Range:%f\tDiameter:%f\n",range,diameter);

                    if (diameter>4.0) {
                        (void)XSetLineAttributes(XtDisplay(da), gc, 2, LineSolid, CapButt,JoinMiter);
                        //(void)XSetForeground(XtDisplay(da),gc,colors[0x0a]);
                        //(void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3
                        (void)XSetForeground(XtDisplay(da),gc,color);

                        (void)XDrawArc(XtDisplay(da),where,gc,
                            (int)(((x_long-x_long_offset)/scale_x)-(diameter/2)),
                            (int)(((y_lat-y_lat_offset)/scale_y)-(diameter/2)),
                            (unsigned int)diameter,(unsigned int)diameter,0,64*360);
                    }
                }
            }
        }
    }
}





void draw_phg_rng(long x_long, long y_lat, char *phg, time_t sec_heard, Pixmap where) {
    double range, diameter;
    int offx,offy;
    long xx,yy,xx1,yy1,fxx,fyy;
    long max_x, max_y;
    double tilt;
    double a,b;
    char is_rng;
    char *strp;

    xx=0l;
    yy=0l;
    xx1=0l;
    yy1=0l;
    fxx=0l;
    fyy=0l;
    tilt=0.0;
    is_rng=0;
    offx=0;
    offy=0;

    if (phg[0] == 'R' && phg[1] == 'N' && phg[2] == 'G')
        is_rng = 1;

    /* max off screen values */
    max_x = screen_width+800l;
    max_y = screen_height+800l;
    if ( ((sec_old+sec_heard)>sec_now()) || show_old_data ) {
        if ((x_long>=0) && (x_long<=129600000l)) {
            if ((y_lat>=0) && (y_lat<=64800000l)) {

// Prevents it from being drawn when the symbol is off-screen.
// It'd be better to check for lat/long +/- range to see if it's on the screen.

                if ((x_long>x_long_offset) && (x_long<(x_long_offset+(long)(screen_width *scale_x)))) {
                    if ((y_lat>y_lat_offset) && (y_lat<(y_lat_offset+(long)(screen_height*scale_y)))) {

                        if (is_rng) {
                            strp = &phg[3];
                            range = atof(strp);
                        }
                        else {
                            range = phg_range(phg[3],phg[4],phg[5]);
                        }

                        // Range is in miles.  Bottom term is in meters before the 0.0006214
                        // multiplication factor which converts it to miles.
                        // Equation is:  2 * ( range(mi) / x-distance across window(mi) )
                        diameter = 2.0 * ( range/
                            (scale_x * calc_dscale_x(mid_x_long_offset,mid_y_lat_offset) * 0.0006214 ) );

                        //printf("PHG: %s, Diameter: %f\n", phg, diameter);

                        a=diameter;
                        b=diameter/2;
                        if (!is_rng)    // Figure out the directivity, if outside range of 0-8 it's declared to be an omni
                            switch (phg[6]-'0') {
                            case(0):
                                offx=0;
                                offy=0;
                                break;

                            case(1):    //  45
                                offx=-1*(diameter/6);
                                offy=diameter/6;
                                tilt=5.49778;
                                break;

                            case(2):    //  90
                                offx=-1*(diameter/6);
                                offy=0;
                                tilt=0;
                                break;

                            case(3):    // 135
                                offx=-1*(diameter/6);
                                offy=-1*(diameter/6);
                                tilt=.78539;
                                break;

                            case(4):    // 180
                                offx=0;
                                offy=-1*(diameter/6);
                                tilt=1.5707;
                                break;

                            case(5):    // 225
                                offx=diameter/6;
                                offy=-1*(diameter/6);
                                tilt=2.3561;
                                break;

                            case(6):    // 270
                                offx=diameter/6;
                                offy=0;
                                tilt=3.14159;
                                break;

                            case(7):    // 315
                                offx=diameter/6;
                                offy=diameter/6;
                                tilt=3.92699;
                                break;

                            case(8):    // 360
                                offx=0;
                                offy=diameter/6;
                                tilt=4.71238;
                                break;

                            default:
                                offx=0;
                                offy=0;
                                break;
                            }
                        //printf("PHG=%02f %0.2f %0.2f %0.2f pix %0.2f\n",range,power,height,gain,diameter);
                        if (diameter>4.0) {
                            (void)XSetLineAttributes(XtDisplay(da), gc, 1, LineSolid, CapButt,JoinMiter);
                            if ((sec_old+sec_heard)>sec_now())
                                (void)XSetForeground(XtDisplay(da),gc,colors[0x0a]);
                            else
                                (void)XSetForeground(XtDisplay(da),gc,colors[0x52]);

                            if (is_rng || phg[6]=='0') {    // Draw circle
                                (void)XDrawArc(XtDisplay(da),where,gc,
                                        (int)(((x_long-x_long_offset)/scale_x)-(diameter/2)),
                                        (int)(((y_lat-y_lat_offset)/scale_y)-(diameter/2)),
                                        (unsigned int)diameter,(unsigned int)diameter,0,64*360);
                            } else {    // Draw oval to depict beam heading

                            // If phg[6] != '0' we still draw a circle, but the center
                            // is offset in the indicated direction by 1/3 the radius.
                                
                            // Draw Circle
                            (void)XDrawArc(XtDisplay(da),where,gc,
                                (int)(((x_long-x_long_offset)/scale_x)-(diameter/2) - offx),
                                (int)(((y_lat-y_lat_offset)/scale_y)-(diameter/2) - offy),
                                (unsigned int)diameter,(unsigned int)diameter,0,64*360);
                            }
                        }
                    }
                }
            }
        }
    }
}





// Function to draw DF circles around objects/stations for DF'ing purposes.
//
// We change from filled circles to open circles at zoom level 128 for speed purposes.
//
void draw_DF_circle(long x_long, long y_lat, char *shgd, time_t sec_heard, Pixmap where) {
    double range, diameter;
    int offx,offy;
    long xx,yy,xx1,yy1,fxx,fyy;
    long max_x, max_y;
    double tilt;
    double a,b;

    xx=0l;
    yy=0l;
    xx1=0l;
    yy1=0l;
    fxx=0l;
    fyy=0l;
    tilt=0.0;
    offx=0;
    offy=0;

    /* max off screen values */
    max_x = screen_width+800l;
    max_y = screen_height+800l;
    if ( ((sec_old+sec_heard)>sec_now()) || show_old_data ) {
        if ((x_long>=0) && (x_long<=129600000l)) {
            if ((y_lat>=0) && (y_lat<=64800000l)) {

// Prevents it from being drawn when the symbol is off-screen.
// It'd be better to check for lat/long +/- range to see if it's on the screen.

                if ((x_long>x_long_offset) && (x_long<(x_long_offset+(long)(screen_width *scale_x)))) {
                    if ((y_lat>y_lat_offset) && (y_lat<(y_lat_offset+(long)(screen_height*scale_y)))) {

                        range = shg_range(shgd[3],shgd[4],shgd[5]);

                        // Range is in miles.  Bottom term is in meters before the 0.0006214
                        // multiplication factor which converts it to miles.
                        // Equation is:  2 * ( range(mi) / x-distance across window(mi) )
                        diameter = 2.0 * ( range/
                            (scale_x * calc_dscale_x(mid_x_long_offset,mid_y_lat_offset) * 0.0006214 ) );

                        //printf("PHG: %s, Diameter: %f\n", shgd, diameter);

                        a=diameter;
                        b=diameter/2;
                        // Figure out the directivity, if outside range of 0-8 it's declared to be an omni
                        switch (shgd[6]-'0') {
                            case(0):
                                offx=0;
                                offy=0;
                                break;

                            case(1):    //  45
                                offx=-1*(diameter/6);
                                offy=diameter/6;
                                tilt=5.49778;
                                break;

                            case(2):    //  90
                                offx=-1*(diameter/6);
                                offy=0;
                                tilt=0;
                                break;

                            case(3):    // 135
                                offx=-1*(diameter/6);
                                offy=-1*(diameter/6);
                                tilt=.78539;
                                break;

                            case(4):    // 180
                                offx=0;
                                offy=-1*(diameter/6);
                                tilt=1.5707;
                                break;

                            case(5):    // 225
                                offx=diameter/6;
                                offy=-1*(diameter/6);
                                tilt=2.3561;
                                break;

                            case(6):    // 270
                                offx=diameter/6;
                                offy=0;
                                tilt=3.14159;
                                break;

                            case(7):    // 315
                                offx=diameter/6;
                                offy=diameter/6;
                                tilt=3.92699;
                                break;

                            case(8):    // 360
                                offx=0;
                                offy=diameter/6;
                                tilt=4.71238;
                                break;

                            default:
                                offx=0;
                                offy=0;
                                break;
                        }
                        //printf("PHG=%02f %0.2f %0.2f %0.2f pix %0.2f\n",range,power,height,gain,diameter);

                        //printf("scale_y: %u\n",scale_y);

                        if (diameter>4.0) {
                            if (scale_y > 128) { // Don't fill in circle if zoomed in too far (too slow!)
                                (void)XSetLineAttributes(XtDisplay(da), gc_stipple, 1, LineSolid, CapButt,JoinMiter);
                            }
                            else {
                                (void)XSetLineAttributes(XtDisplay(da), gc_stipple, 8, LineSolid, CapButt,JoinMiter);                            }

                            // Stipple the area instead of obscuring the map underneath
                            (void)XSetStipple(XtDisplay(da), gc_stipple, pixmap_50pct_stipple);
                            (void)XSetFillStyle(XtDisplay(da), gc_stipple, FillStippled);

                            // Choose the color for the DF'ing circle
                            // We try to choose similar colors to those used in DOSaprs,
                            // which are qbasic or gwbasic colors.
                            switch (shgd[3]) {
                                case '9':   // Light Red
                                    if ((sec_old+sec_heard)>sec_now())  // New
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x25]);
                                    else                                // Old
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x25]);
                                   break;
                                case '8':   // Red
                                    if ((sec_old+sec_heard)>sec_now())  // New
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x2d]);
                                    else                                // Old
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x2d]);
                                   break;
                                case '7':   // Light Magenta
                                    if ((sec_old+sec_heard)>sec_now())  // New
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x26]);
                                    else                                // Old
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x26]);
                                   break;
                                case '6':   // Magenta
                                    if ((sec_old+sec_heard)>sec_now())  // New
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x2e]);
                                    else                                // Old
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x2e]);
                             break;
                                case '5':   // Light Cyan
                                    if ((sec_old+sec_heard)>sec_now())  // New
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x24]);
                                    else                                // Old
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x24]);
                              break;
                                case '4':   // Cyan
                                    if ((sec_old+sec_heard)>sec_now())  // New
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x2c]);
                                    else                                // Old
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x2c]);
                               break;
                                case '3':   // White
                                    if ((sec_old+sec_heard)>sec_now())  // New
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x0f]);
                                    else                                // Old
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x0f]);
                                break;
                                case '2':   // Light Blue
                                    if ((sec_old+sec_heard)>sec_now())  // New
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x22]);
                                    else                                // Old
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x22]);
                                 break;
                                case '1':   // Blue
                                    if ((sec_old+sec_heard)>sec_now())  // New
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x2a]);
                                    else                                // Old
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x2a]);
                                  break;
                                case '0':   // DarkGray (APRSdos) or Black (looks better). We use Black.
                                default:
                                    if ((sec_old+sec_heard)>sec_now())  // New (was 0x30)
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x08]);
                                    else                                // Old
                                        (void)XSetForeground(XtDisplay(da),gc_stipple,colors[0x08]);
                                   break;
                            }

                            // If shgd[6] != '0' we still draw a circle, but the center
                            // is offset in the indicated direction by 1/3 the radius.
                                
                            // Draw Circle
                            (void)XDrawArc(XtDisplay(da),where,gc_stipple,
                                    (int)(((x_long-x_long_offset)/scale_x)-(diameter/2) - offx),
                                    (int)(((y_lat-y_lat_offset)/scale_y)-(diameter/2) - offy),
                                    (unsigned int)diameter,(unsigned int)diameter,0,64*360);

                            if (scale_y > 128) { // Don't fill in circle if zoomed in too far (too slow!)
                                while (diameter > 1.0) {
                                    diameter = diameter - 1.0;
                                    (void)XDrawArc(XtDisplay(da),where,gc_stipple,
                                        (int)(((x_long-x_long_offset)/scale_x)-(diameter/2) - offx),
                                        (int)(((y_lat-y_lat_offset)/scale_y)-(diameter/2) - offy),
                                        (unsigned int)diameter,(unsigned int)diameter,0,64*360);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    // Change back to non-stipple for whatever drawing occurs after this
    (void)XSetFillStyle(XtDisplay(da), gc_stipple, FillSolid);
}





#define BARB_LEN 15
#define BARB_SPACING 15
void draw_half_barbs(int *i, int quantity, float bearing_radians, long x, long y, char *course, Pixmap where) {
    float barb_radians = bearing_radians + ( (45/360.0) * 2.0 * M_PI);
    int j;
    long start_x, start_y, off_x, off_y;


    for (j = 0; j < quantity; j++) {
        // Starting point for barb is (*i * BARB_SPACING) pixels
        // along bearing_radians vector
        *i = *i + BARB_SPACING;
        off_x = *i * cos(bearing_radians);
        off_y = *i * sin(bearing_radians);
        start_y = y + off_y;
        start_x = x + off_x;

        // Set off in the barb direction now
        off_y = (long)( (BARB_LEN / 2) * sin(barb_radians) );
        off_x = (long)( (BARB_LEN / 2) * cos(barb_radians) );

        (void)XSetLineAttributes(XtDisplay(da), gc, 0, LineSolid, CapButt,JoinMiter);
        (void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3
        (void)XDrawLine(XtDisplay(da),where,gc,
            start_x,
            start_y,
            start_x + off_x,
            start_y + off_y);
    }
}





void draw_full_barbs(int *i, int quantity, float bearing_radians, long x, long y, char *course, Pixmap where) {
    float barb_radians = bearing_radians + ( (45/360.0) * 2.0 * M_PI);
    int j;
    long start_x, start_y, off_x, off_y;


    for (j = 0; j < quantity; j++) {
        // Starting point for barb is (*i * BARB_SPACING) pixels
        // along bearing_radians vector
        *i = *i + BARB_SPACING;
        off_x = *i * cos(bearing_radians);
        off_y = *i * sin(bearing_radians);
        start_y = y + off_y;
        start_x = x + off_x;

        // Set off in the barb direction now
        off_y = (long)( BARB_LEN * sin(barb_radians) );
        off_x = (long)( BARB_LEN * cos(barb_radians) );

        (void)XSetLineAttributes(XtDisplay(da), gc, 0, LineSolid, CapButt,JoinMiter);
        (void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3
        (void)XDrawLine(XtDisplay(da),where,gc,
            start_x,
            start_y,
            start_x + off_x,
            start_y + off_y);
    }
}





void draw_triangle_flags(int *i, int quantity, float bearing_radians, long x, long y, char *course, Pixmap where) {
    float barb_radians = bearing_radians + ( (45/360.0) * 2.0 * M_PI);
    int j;
    long start_x, start_y, off_x, off_y, off_x2, off_y2;
    XPoint points[3];


    for (j = 0; j < quantity; j++) {
        // Starting point for barb is (*i * BARB_SPACING) pixels
        // along bearing_radians vector
        *i = *i + BARB_SPACING;
        off_x = *i * cos(bearing_radians);
        off_y = *i * sin(bearing_radians);
        start_y = y + off_y;
        start_x = x + off_x;

        // Calculate 2nd point along staff
        off_x2 = (BARB_SPACING/2) * cos(bearing_radians);
        off_y2 = (BARB_SPACING/2) * sin(bearing_radians);

        // Set off in the barb direction now
        off_y = (long)( BARB_LEN * sin(barb_radians) );
        off_x = (long)( BARB_LEN * cos(barb_radians) );

        (void)XSetLineAttributes(XtDisplay(da), gc, 0, LineSolid, CapButt,JoinMiter);
        (void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3

        points[0].x = start_x;           points[0].y = start_y;
        points[1].x = start_x + off_x;   points[1].y = start_y + off_y;
        points[2].x = start_x + off_x2;  points[2].y = start_y + off_y2;
        (void)XFillPolygon(XtDisplay(da), where, gc, points, 3, Convex, CoordModeOrigin);
    }
}





void draw_square_flags(int *i, int quantity, float bearing_radians, long x, long y, char *course, Pixmap where) {
    float barb_radians = bearing_radians + ( (90/360.0) * 2.0 * M_PI);
    int j;
    long start_x, start_y, off_x, off_y, off_x2, off_y2;
    XPoint points[4];


    for (j = 0; j < quantity; j++) {
        // Starting point for barb is (*i * BARB_SPACING) pixels
        // along bearing_radians vector
        *i = *i + BARB_SPACING;
        off_x = *i * cos(bearing_radians);
        off_y = *i * sin(bearing_radians);
        start_y = y + off_y;
        start_x = x + off_x;

        // Calculate 2nd point along staff
        off_x2 = (BARB_SPACING/2) * cos(bearing_radians);
        off_y2 = (BARB_SPACING/2) * sin(bearing_radians);

        // Set off in the barb direction now
        off_y = (long)( BARB_LEN * sin(barb_radians) );
        off_x = (long)( BARB_LEN * cos(barb_radians) );

        (void)XSetLineAttributes(XtDisplay(da), gc, 0, LineSolid, CapButt,JoinMiter);
        (void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3

        points[0].x = start_x;                   points[0].y = start_y;
        points[1].x = start_x + off_x;           points[1].y = start_y + off_y;
        points[2].x = start_x + off_x + off_x2;  points[2].y = start_y + off_y + off_y2;
        points[3].x = start_x + off_x2;          points[3].y = start_y + off_y2;
        (void)XFillPolygon(XtDisplay(da), where, gc, points, 4, Convex, CoordModeOrigin);
    }
}





// Function to draw wind barbs.  Use speed in knots to determine the
// flags and barbs to draw along the shaft.  Course is in true
// degrees, in the direction that the wind is coming from.
//
//   Square flag = 100 knots
// Triangle flag = 50 knots
//     Full barb = 10 knots
//     Half barb = 5 knots
//
void draw_wind_barb(long x_long, long y_lat, char *speed,
        char *course, time_t sec_heard, Pixmap where) {
    int square_flags = 0;
    int triangle_flags = 0;
    int full_barbs = 0;
    int half_barbs = 0;
    int shaft_length = 0;
    int my_speed = atoi(speed);     // In mph (so far)
    int my_course = atoi(course);   // In ° true
    float bearing_radians;
    long off_x,off_y;
    long x,y;
    int i;


// Ghost the wind barb if sec_heard is too long.
// (TBD)


    // Refuse to draw the barb at all if we're past the "clear"
    // interval:
    if ( ((sec_clear+sec_heard)>sec_now()) || show_old_data ) {

 
// What to do if my_speed is zero?  Blank out any wind barbs
// that were written before?


        // Convert from mph to knots for wind speed.
        my_speed = my_speed * 0.8689607;

        //printf("mph:%s, knots:%d\n",speed,my_speed);

        // Adjust so that it fits our screen angles.  We're off by
        // 90 degrees.
        my_course = (my_course - 90) % 360;

        square_flags = (int)(my_speed / 100);
        my_speed = my_speed % 100;

        triangle_flags = (int)(my_speed / 50);
        my_speed = my_speed % 50;

        full_barbs = (int)(my_speed / 10);
        my_speed = my_speed % 10;

        half_barbs = (int)(my_speed / 5);

        shaft_length = BARB_SPACING * (square_flags + triangle_flags + full_barbs
            + half_barbs + 1);

        // Set a minimum length for the shaft?
        if (shaft_length < 20)
            shaft_length = 20;

        if (debug_level & 128) {
            printf("Course:%d,\tL:%d,\tsq:%d,\ttr:%d,\tfull:%d,\thalf:%d\n",
                atoi(course),
                shaft_length,
                square_flags,
                triangle_flags,
                full_barbs,
                half_barbs);
        }

        // Draw shaft at proper angle.
        bearing_radians = (my_course/360.0) * 2.0 * M_PI;

        off_y = (long)( shaft_length * sin(bearing_radians) );
        off_x = (long)( shaft_length * cos(bearing_radians) );

        x = (x_long - x_long_offset)/scale_x;
        y = (y_lat - y_lat_offset)/scale_y;

        (void)XSetLineAttributes(XtDisplay(da), gc, 0, LineSolid, CapButt,JoinMiter);
        (void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3
        (void)XDrawLine(XtDisplay(da),where,gc,
            x,
            y,
            x + off_x,
            y + off_y);

        // Increment along shaft and draw filled polygons at:
        // "(angle + 45) % 360" degrees to create flags.

        i = BARB_SPACING;
        // Draw half barbs if any
        if (half_barbs)
        	draw_half_barbs(&i,
                half_barbs,
                bearing_radians,
                x,
                y,
                course,
                where);
 
        // Draw full barbs if any
        if (full_barbs)
        	draw_full_barbs(&i,
                full_barbs,
                bearing_radians,
                x,
                y,
                course,
                where);
 
        // Draw triangle flags if any
        if (triangle_flags)
        	draw_triangle_flags(&i,
                triangle_flags,
                bearing_radians,
                x,
                y,
                course,
                where);
 
        // Draw rectangle flags if any
        if (square_flags)
        	draw_square_flags(&i,
                square_flags,
                bearing_radians,
                x,
                y,
                course,
                where);
    }
}





// Function to draw beam headings for DF'ing purposes.  Separates NRQ into its
// components, which are Number/Range/Quality.
//
// If N is 0, then the NRQ value is meaningless.  1 through 8 are hits per period
// of time (auto-DF'ing equipment).  A value of 8 means all samples possible got
// a hit.  A value of 9 means that the report is manual.
//
// Range limits the length of the line to the original map's scale of the sending
// station.  The range is 2**R, so for R=4 the range would be 16 miles.
//
// Q is a single digit from 0-9 and provides indication of bearing accuracy:
// 0 Useless
// 1 <240 deg (worst)
// 2 <120 deg
// 3 <64  deg
// 4 <32  deg
// 5 <16  deg
// 6 <8   deg
// 7 <4   deg
// 8 <2   deg
// 9 <1   deg (best)
//
//
// TODO: Should we draw with XOR for this function?  Would appear on most maps that
// way, and we wouldn't have to worry much about color.
//
// TODO: If Q between 1 and 8, shade the entire area to show the beam width?
//
//
// Latest:  We ignore the color parameter and draw everything using
// red3.
//
void draw_bearing(long x_long, long y_lat, char *course,
        char *bearing, char *NRQ, int color,
        time_t sec_heard, Pixmap where) {
    int range = 0;
    double screen_miles;
    float bearing_radians_min = 0.0;
    float bearing_radians_max = 0.0;
    long offx_min, offx_max, offy_min, offy_max;
    double offx_miles_min, offx_miles_max, offy_miles_min, offy_miles_max;
    long max_x, max_y;
    int real_bearing = 0;
    int real_bearing_min = 0;
    int real_bearing_max = 0;
    int width = 0;


    // Check for a zero value for N.  If found, the NRQ value is meaningless
    // and we need to assume some working default values.
    if (NRQ[0] != '0') {

        range = (int)( pow(2.0,NRQ[1] - '0') );

        switch (NRQ[2]) {
            case('1'):
                width = 240;    // Degrees of beam width.  What's the point?
                break;
            case('2'):
                width = 120;    // Degrees of beam width.  What's the point?
                break;
            case('3'):
                width = 64; // Degrees of beam width.  What's the point?
                break;
            case('4'):
                width = 32; // Degrees of beam width.  Starting to be usable.
                break;
            case('5'):
                width = 16; // Degrees of beam width.  Usable.
                break;
            case('6'):
                width = 8;  // Degrees of beam width.  Usable.
                break;
            case('7'):
                width = 4;  // Degrees of beam width.  Nice!
                break;
            case('8'):
                width = 2;  // Degrees of beam width.  Nice!
                break;
            case('9'):
                width = 1;  // Degrees of beam width.  Very Nice!
                break;
            default:
                width = 8;  // Degrees of beam width
                break;
        }
    }
    else {  // Assume some default values.
        range = 512;  // Assume max range of 512 miles
        width = 8;      // Assume 8 degrees for beam width
    }

    // We have the course of the vehicle and the bearing from the vehicle.
    // Now we need the real bearing.
    if (atoi(course) != 0) {
        real_bearing = (atoi(course) + atoi(bearing) + 270) % 360;
        real_bearing_min = (int)(real_bearing + 360 - (width/2.0)) % 360;
        real_bearing_max = (int)(real_bearing + (width/2.0)) % 360;
    }
    else {
        real_bearing = (atoi(bearing) + 270) % 360;
        real_bearing_min = (int)(real_bearing + 360 - (width/2.0)) % 360;
        real_bearing_max = (int)(real_bearing + (width/2.0)) % 360;
    }

    bearing_radians_min = (real_bearing_min/360.0) * 2.0 * M_PI;
    bearing_radians_max = (real_bearing_max/360.0) * 2.0 * M_PI;

    //printf("Bearing: %i degrees, Range: %i miles, Width: %i degree(s), Radians: %f, ",
    //        real_bearing, range, width, bearing_radians); 

    // Range is in miles.  Bottom term is in meters before the 0.0006214
    // multiplication factor which converts it to miles.
    // Equation is:  x-distance across window(mi)
    screen_miles = scale_x * calc_dscale_x(mid_x_long_offset,mid_y_lat_offset) * .6214;

    // Shorten range to more closely fit the screen
    if ( range > (3.0 * screen_miles) )
        range = 3.0 * screen_miles;

    offy_miles_min = sin(bearing_radians_min) * range;
    offy_miles_max = sin(bearing_radians_max) * range;

    offx_miles_min = cos(bearing_radians_min) * range;
    offx_miles_max = cos(bearing_radians_max) * range;


    // TODO:  Needs to be a calc_dscale_y for below:
    offy_min = (long)(offy_miles_min / (scale_y * calc_dscale_x(mid_x_long_offset,mid_y_lat_offset) * 0.0006214) );
    offy_max = (long)(offy_miles_max / (scale_y * calc_dscale_x(mid_x_long_offset,mid_y_lat_offset) * 0.0006214) );

    offx_min = (long)(offx_miles_min / (scale_x * calc_dscale_x(mid_x_long_offset,mid_y_lat_offset) * 0.0006214) );
    offx_max = (long)(offx_miles_max / (scale_x * calc_dscale_x(mid_x_long_offset,mid_y_lat_offset) * 0.0006214) );


    /*
    if (offx > 10000)
        offx = 10000;
    if (offx < -10000)
        offx = -10000;
    if (offy > 10000)
        offy = 10000;
    if (offy < -10000)
        offy = -10000;
    */


    //printf("offx_min: %li, offx_max: %li, offy_min: %li, offy_max: %li\n", offx_min, offx_max, offy_min, offy_max);

    //printf("X miles: %f, Y miles: %f, screen_miles: %f\n",
    //    offx_miles,
    //    offy_miles,
    //    screen_miles);

    /* max off screen values */
    max_x = screen_width+800l;
    max_y = screen_height+800l;
    if ( ((sec_old+sec_heard)>sec_now()) || show_old_data ) {
        if ((x_long>=0) && (x_long<=129600000l)) {
            if ((y_lat>=0) && (y_lat<=64800000l)) {

// Prevents it from being drawn when the symbol is off-screen.
// It'd be better to check for lat/long +/- range to see if it's on the screen.

/*
                if ((x_long>x_long_offset) && (x_long<(x_long_offset+(long)(screen_width *scale_x)))) {
                    if ((y_lat>y_lat_offset) && (y_lat<(y_lat_offset+(long)(screen_height*scale_y)))) {
*/
                        (void)XSetLineAttributes(XtDisplay(da), gc, 2, LineSolid, CapButt,JoinMiter);
                        //(void)XSetForeground(XtDisplay(da),gc,colors[0x0a]);
                        (void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3


//                        (void)XSetLineAttributes(XtDisplay(da), gc_tint, 2, LineSolid, CapRound, JoinRound);


//                        (void)XSetFunction (XtDisplay (da), gc_tint, GXor);
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



                        // Stipple the area instead of obscuring the map underneath
//                        (void)XSetStipple(XtDisplay(da), gc_tint, pixmap_2x2_stipple);
//                        (void)XSetFillStyle(XtDisplay(da), gc_tint, FillStippled);

                        if ((sec_old+sec_heard)>sec_now()) {  // New
//                            (void)XSetForeground(XtDisplay(da),gc_tint,color);
//                            (void)XSetForeground(XtDisplay(da),gc,color);
                        }
                        else {                                // Old
//                            (void)XSetForeground(XtDisplay(da),gc_tint,color);
//                            (void)XSetForeground(XtDisplay(da),gc,color);
                        }


//                        (void)XDrawLine(XtDisplay(da),where,gc_tint,
                        (void)XDrawLine(XtDisplay(da),where,gc,
                                (x_long-x_long_offset)/scale_x,
                                (y_lat-y_lat_offset)/scale_y,
                                ((x_long-x_long_offset)/scale_x + offx_min),
                                ((y_lat-y_lat_offset)/scale_y) + offy_min);

//                        (void)XDrawLine(XtDisplay(da),where,gc_tint,
                        (void)XDrawLine(XtDisplay(da),where,gc,
                                (x_long-x_long_offset)/scale_x,
                                (y_lat-y_lat_offset)/scale_y,
                                ((x_long-x_long_offset)/scale_x + offx_max),
                                ((y_lat-y_lat_offset)/scale_y) + offy_max);
/*
                    }
                }
*/
            }
        }
    }
    // Change back to non-stipple for whatever drawing occurs after this
//    (void)XSetFillStyle(XtDisplay(da), gc_tint, FillSolid);
}





void draw_ambiguity(long x_long, long y_lat, char amb, time_t sec_heard, Pixmap where) {
    long left, top, offset_lat, offset_long;
    int scale_limit;

    switch (amb) {
    case 1: // +- 1/20th minute
        offset_lat = offset_long = 360000.0 * 0.05 / 60.0;
        scale_limit = 32;
        break;
    case 2: // +- 1/2 minute
        offset_lat = offset_long = 360000.0 * 0.5 / 60.0;
        scale_limit = 256;
        break;
    case 3: // +- 5 minutes
        offset_lat = offset_long = 360000.0 * 5.0 / 60.0;
        scale_limit = 2048;
        break;
    case 4: // +- 1/2 degree
        offset_lat = offset_long = 360000.0 * 0.5;
        scale_limit = 16384;
        break;
    case 5: // grid square: 2.5min lat x 5min lon
        offset_lat  = 360000.0 * 1.25 / 60.0;
        offset_long = 360000.0 * 2.50 / 60.0;
        scale_limit = 1024;
        break;
    case 6: // grid square: 1deg lat x 2deg lon
        offset_lat  = 360000.0 * 0.5;
        offset_long = 360000.0 * 1.0;
        scale_limit = 16384;
        break;
    case 0:
    default:
        return; // if no ambiguity, do nothing
        break;
    }
    if (scale_y > scale_limit)
        return; // the box would be about the size of the symbol

    left = (x_long - x_long_offset - offset_long) / scale_x;
    top  = (y_lat  - y_lat_offset  - offset_lat)  / scale_y;

    if ( ((sec_old+sec_heard)>sec_now()) || show_old_data ) {
        if ((x_long>=0) && (x_long<=129600000l)) {
            if ((y_lat>=0) && (y_lat<=64800000l)) {

// Prevents it from being drawn when the symbol is off-screen.
// It'd be better to check for lat/long +/- range to see if it's on the screen.

                if ((x_long>x_long_offset) && (x_long<(x_long_offset+(long)(screen_width *scale_x)))) {
                    if ((y_lat>y_lat_offset) && (y_lat<(y_lat_offset+(long)(screen_height*scale_y)))) {
			long width, height;
			width  = (2*offset_long)/scale_x;
			height = (2*offset_lat)/scale_y;

                        (void)XSetForeground(XtDisplay(da), gc, colors[0x08]);
			if      (width  > 200 || height > 200)
			    (void)XSetStipple(XtDisplay(da), gc, pixmap_13pct_stipple);
			else if (width  > 100 || height > 100)
			    (void)XSetStipple(XtDisplay(da), gc, pixmap_25pct_stipple);
			else
			    (void)XSetStipple(XtDisplay(da), gc, pixmap_50pct_stipple);
                        (void)XSetFillStyle(XtDisplay(da), gc, FillStippled);
                        (void)XFillRectangle(XtDisplay(da), where, gc,
                                             left, top, width, height);
                        (void)XSetFillStyle(XtDisplay(da), gc, FillSolid);
                    }
                }
            }
        }
    }
}





static __inline__ int onscreen(long left, long right, long top, long bottom) {
    // This checks to see if any part of a box is on the screen.
    if (left > screen_width || right < 0 || top > screen_height || bottom < 0)
        return 0;
    else
        return 1;
}





static __inline__ short l16(long val) {
    // This limits large integer values to the 16 bit range for X drawing
    if (val < -32768) val = -32768;
    if (val >  32767) val =  32767;
    return (short)val;
}





void draw_area(long x_long, long y_lat, char type, char color,
               char sqrt_lat_off, char sqrt_lon_off, char width, time_t sec_heard, Pixmap where) {
    long left, top, right, bottom, xoff, yoff;
    int  c;
    XPoint points[4];

    if ( ( ((sec_clear + sec_heard) <= sec_now()) && !show_old_data ) ||
        (x_long < 0) || (x_long > 129600000l) ||
        (y_lat  < 0) || (y_lat  > 64800000l) )
        return;

    xoff = 360000.0 / 1500.0 * (sqrt_lon_off * sqrt_lon_off) / scale_x;
    yoff = 360000.0 / 1500.0 * (sqrt_lat_off * sqrt_lat_off) / scale_y;
    right  = (x_long - x_long_offset) / scale_x;
    bottom = (y_lat  - y_lat_offset)  / scale_y;
    left   = right  - xoff;
    top    = bottom - yoff;

    // colors[0x21] is the first in the list of area object colors in main.c
    c = colors[0x21 + color];

    (void)XSetForeground(XtDisplay(da), gc, c);
    if (xoff < 20 || yoff < 20)
        (void)XSetLineAttributes(XtDisplay(da), gc, 1, LineSolid, CapButt,JoinMiter);
    else
        (void)XSetLineAttributes(XtDisplay(da), gc, 2, LineSolid, CapButt,JoinMiter);
    (void)XSetFillStyle(XtDisplay(da), gc, FillSolid); // just in case
    (void)XSetStipple(XtDisplay(da), gc, pixmap_50pct_stipple);

    switch (type) {
    case AREA_OPEN_BOX:
        if (onscreen(left, right, top, bottom))
            (void)XDrawRectangle(XtDisplay(da), where, gc, l16(left), l16(top), l16(xoff), l16(yoff));
        break;
    case AREA_FILLED_BOX:
        if (onscreen(left, right, top, bottom)) {
            (void)XSetFillStyle(XtDisplay(da), gc, FillStippled);
            (void)XFillRectangle(XtDisplay(da), where, gc, l16(left), l16(top), l16(xoff), l16(yoff));
        }
        break;
    /* For the rest of the objects, the l16 limiting of the values is inadequate because the
       shapes will still be draw wrong if the value actually was limited down.
       However, this is slightly better than passing long or int values that would just be
       used as 16bit by X (i.e.: truncated) until I/we come up with some sort of clipping algorithm.
       In real use, what I'm talking about will occur based on two things: the size of the area and
       the zoom level being used.  The more the extents of the area go beyond the edges of the screen,
       the more this will happen. N7TAP */
    case AREA_OPEN_CIRCLE:
        right  += xoff;
        bottom += yoff;
        if (onscreen(left, right, top, bottom))
            (void)XDrawArc(XtDisplay(da), where, gc, l16(left), l16(top), l16(2*xoff), l16(2*yoff), 0, 64 * 360);
        break;
    case AREA_FILLED_CIRCLE:
        right  += xoff;
        bottom += yoff;
        if (onscreen(left, right, top, bottom)) {
            (void)XSetFillStyle(XtDisplay(da), gc, FillStippled);
            (void)XFillArc(XtDisplay(da), where, gc, l16(left), l16(top), l16(2*xoff), l16(2*yoff), 0, 64 * 360);
        }
        break;
    case AREA_LINE_RIGHT:
        left  += xoff;
        right += xoff;
        if (width > 0) {
            double angle = atan((float)xoff/(float)yoff);
            int conv_width = width/(scale_x*calc_dscale_x(mid_x_long_offset,mid_y_lat_offset)*0.0006214);
            points[0].x = l16(left-(conv_width * cos(angle))+xoff);
            points[0].y = l16(top -(conv_width * sin(angle)));
            points[1].x = l16(left-(conv_width * cos(angle)));
            points[1].y = l16(top -(conv_width * sin(angle))+yoff);
            points[2].x = l16(left+(conv_width * cos(angle)));
            points[2].y = l16(top +(conv_width * sin(angle))+yoff);
            points[3].x = l16(left+(conv_width * cos(angle))+xoff);
            points[3].y = l16(top +(conv_width * sin(angle)));
            if (onscreen(points[1].x, points[3].x, points[0].y, points[2].y)) {
                (void)XSetFillStyle(XtDisplay(da), gc, FillStippled);
                (void)XFillPolygon(XtDisplay(da), where, gc, points, 4, Convex, CoordModeOrigin);
            }
        }
        if (onscreen(left, right, top, bottom)) {
            (void)XSetFillStyle(XtDisplay(da), gc, FillSolid);
            (void)XDrawLine(XtDisplay(da), where, gc, l16(left), l16(bottom), l16(right), l16(top));
        }
        break;
    case AREA_LINE_LEFT:
        if (width > 0) {
            double angle = atan((float)xoff/(float)yoff);
            int conv_width = width/(scale_x*calc_dscale_x(mid_x_long_offset,mid_y_lat_offset)*0.0006214);
            points[0].x = l16(left+(conv_width * cos(angle)));
            points[0].y = l16(top -(conv_width * sin(angle)));
            points[1].x = l16(left+(conv_width * cos(angle))+xoff);
            points[1].y = l16(top -(conv_width * sin(angle))+yoff);
            points[2].x = l16(left-(conv_width * cos(angle))+xoff);
            points[2].y = l16(top +(conv_width * sin(angle))+yoff);
            points[3].x = l16(left-(conv_width * cos(angle)));
            points[3].y = l16(top +(conv_width * sin(angle)));
            if (onscreen(points[3].x, points[1].x, points[0].y, points[2].y)) {
                (void)XSetFillStyle(XtDisplay(da), gc, FillStippled);
                (void)XFillPolygon(XtDisplay(da), where, gc, points, 4, Convex, CoordModeOrigin);
            }
        }
        if (onscreen(left, right, top, bottom)) {
            (void)XSetFillStyle(XtDisplay(da), gc, FillSolid);
            (void)XDrawLine(XtDisplay(da), where, gc, l16(right), l16(bottom), l16(left), l16(top));
        }
        break;
    case AREA_OPEN_TRIANGLE:
        left -= xoff;
        points[0].x = l16(right);     points[0].y = l16(bottom);
        points[1].x = l16(left+xoff); points[1].y = l16(top);
        points[2].x = l16(left);      points[2].y = l16(bottom);
        points[3].x = l16(right);     points[3].y = l16(bottom);
        if (onscreen(left, right, top, bottom))
            (void)XDrawLines(XtDisplay(da), where, gc, points, 4, CoordModeOrigin);
        break;
    case AREA_FILLED_TRIANGLE:
        left -= xoff;
        points[0].x = l16(right);     points[0].y = l16(bottom);
        points[1].x = l16(left+xoff); points[1].y = l16(top);
        points[2].x = l16(left);      points[2].y = l16(bottom);
        if (onscreen(left, right, top, bottom)) {
            (void)XSetFillStyle(XtDisplay(da), gc, FillStippled);
            (void)XFillPolygon(XtDisplay(da), where, gc, points, 3, Convex, CoordModeOrigin);
        }
        break;
    // Not implemented yet, because no guidance on how (no response from aprsspec list)
    case AREA_OPEN_ELLIPSE:
    case AREA_FILLED_ELLIPSE:
        break;
    }
    (void)XSetFillStyle(XtDisplay(da), gc, FillSolid);
}





/* DK7IN: Statistics for colors in all symbols (as of 16.03.2001)
60167 .
 6399 q
 3686 m
 3045 c
 2034 j
 1903 h
 1726 l
 1570 k
 1063 g
 1051 #
  840 p
  600 ~
  477 i
  443 n
  430 a
  403 o
  337 f
  250 b
  207 e
  169 d
*/





// read pixels from file, speeding it up by smart ordering of switches
void read_symbol_from_file(FILE *f, char *pixels) {
    int x,y;                
    int color;
    char line[21];

    for (y=0;y<20;y++) {
        (void)get_line(f,line,21);
        for (x=0;x<20;x++) {
            switch (line[x]) {
                case('.'):       // transparent
                    color=0xff;
                    break;
                case('q'):       // #000000  black   0%
                    color=0x51;
                    break;
                case('m'):       // #FFFFFF  white 100%
                    color=0x4d;
                    break;
                case('c'):       // #CCCCCC  gray80 80%
                    color=0x43;
                    break;
                case('j'):       // #EE0000  red2
                    color=0x4a;
                    break;
                case('h'):       // #00BFFF  Deep sky blue
                    color=0x48;
                    break;
                case('l'):       // #0000CD  mediumblue
                    color=0x4c;
                    break;
                case('k'):       // #00CD00  green3
                    color=0x4b;
                    break;
                case('g'):       // #00008B  blue4
                    color=0x47;
                    break;
                case('#'):       // #FFFF00  yellow
                    color=0x40;
                    break;
                case('p'):       // #454545  gray27 27%
                    color=0x50;
                    break;
                case('~'):       // used in the last two symbols in the file
                    color=0xff;  // what should it be? was transparent before...
                    break;
                case('i'):       // #006400  Dark Green
                    color=0x49;
                    break;
                case('n'):       // #878787  gray53 52%
                    color=0x4e;
                    break;
                case('a'):       // #CD6500  darkorange2
                    color=0x41;
                    break;
                case('o'):       // #5A5A5A  gray59 35%
                    color=0x4f;
                    break;
                case('f'):       // #CD3333  brown3
                    color=0x46;
                    break;
                case('b'):       // #A020F0  purple
                    color=0x42;
                    break;
                case('e'):       // #FF4040  brown1
                    color=0x45;
                    break;
                case('d'):       // #CD0000  red3
                    color=0x44;
                    break;
                case('r'):       //          LimeGreen  DK7IN: saw this in the color definitions...
                    color=0x52;                         // so we could use it
                    break;
                default:
                    color=0xff;
                    break;
            }
            pixels[y*20+x] = (char)(color);
        }
    }
}





/* read in symbol table */
void load_pixmap_symbol_file(char *filename) {
    FILE *f;
    char filen[500];
    char line[21];
    char table_char;
    char symbol_char;
    int done;
    char pixels[400];
    char orient;

    busy_cursor(appshell);
    symbols_loaded = 0;
    table_char = '\0';
    symbol_char = '\0';
    done = 0;
    xastir_snprintf(filen, sizeof(filen), "%s/%s", SYMBOLS_DIR, filename);
    f = fopen(filen,"r");
    if (f!=NULL) {
        while (!feof(f) && !done) {
            (void)get_line(f,line,21);
            if (strncasecmp("TABLE ",line,6)==0) {
                table_char=line[6];
                /*printf("TABLE %c\n",table_char);*/
            } else {
                if (strncasecmp("DONE",line,4)==0) {
                    done=1;
                    /*printf("DONE\n");*/
                } else {
                    if (strncasecmp("APRS ",line,5)==0) {
                        symbol_char=line[5];
                        if (strlen(line)>=20 && line[19] == 'l')     // symbol with orientation ?
                            orient = 'l';   // should be 'l' for left
                        else
                            orient = ' ';
                        read_symbol_from_file(f, pixels);                         // read pixels for one symbol
                        insert_symbol(table_char,symbol_char,pixels,270,orient);  // always have normal orientation
                        if (orient == 'l') {
                            insert_symbol(table_char,symbol_char,pixels,  0,'u'); // create other orientations
                            insert_symbol(table_char,symbol_char,pixels, 90,'r');
                            insert_symbol(table_char,symbol_char,pixels,180,'d');
                        }
                    }
                }
            }
        }
    } else
        printf("Error opening symbol file %s\n",filen);
    if (f != NULL)
        (void)fclose(f);
}





/* add a symbol to the end of the symbol table */
void insert_symbol(char table, char symbol, char *pixel, int deg, char orient) {
    int x,y,idx,old_next,color;

    if (symbols_loaded < MAX_SYMBOLS) {
        symbol_data[symbols_loaded].pix=XCreatePixmap(XtDisplay(appshell),
                RootWindowOfScreen(XtScreen(appshell)),
                20,
                20,
                DefaultDepthOfScreen(XtScreen(appshell)));

        symbol_data[symbols_loaded].pix_mask=XCreatePixmap(XtDisplay(appshell),
                RootWindowOfScreen(XtScreen(appshell)),
                20,
                20,
                1);

        symbol_data[symbols_loaded].pix_mask_old=XCreatePixmap(XtDisplay(appshell),
                RootWindowOfScreen(XtScreen(appshell)),
                20,
                20,
                1);

        old_next=0;
        for (y=0;y<20;y++) {
            for (x=0;x<20;x++) {
                switch (deg) {
                    case(0):
                        idx = 20* (19-x) +   y;
                        break;
                    case(90):
                        idx = 20*   y    + (19-x);
                        break;
                    case(180):
                        idx = 20* (19-x) + (19-y);
                        break;
                    default:
                        idx = 20*   y    +   x;
                        break;
                }
                color = (int)(pixel[idx]);
                if (color<0)
                    color = 0xff;

                // DK7IN: is (da) correct or should this be (appshell) ?
                (void)XSetForeground(XtDisplay(da),gc,colors[color]);
                (void)XDrawPoint(XtDisplay(da),symbol_data[symbols_loaded].pix,gc,x,y);
                // DK7IN

                if (color != 0xff)     // create symbol mask
                    (void)XSetForeground(XtDisplay(appshell),gc2,1);  // active bit
                else
                    (void)XSetForeground(XtDisplay(appshell),gc2,0);  // transparent

                (void)XDrawPoint(XtDisplay(appshell),symbol_data[symbols_loaded].pix_mask,gc2,x,y);

                old_next++;        // create ghost symbol mask
                if (old_next>1) {  // by setting every 2nd bit to transparent
                    old_next=0;
                    (void)XSetForeground(XtDisplay(appshell),gc2,0);
                }
                (void)XDrawPoint(XtDisplay(appshell),symbol_data[symbols_loaded].pix_mask_old,gc2,x,y);
            }
            old_next++;    // shift one bit every scan line for ghost image
            if (old_next>1)
                old_next=0;
        }
        symbol_data[symbols_loaded].active = SYMBOL_ACTIVE;
        symbol_data[symbols_loaded].table  = table;
        symbol_data[symbols_loaded].symbol = symbol;
        symbol_data[symbols_loaded].orient = orient;
        symbols_loaded++;
    }
}





/* calculate symbol orientation from course */
char symbol_orient(char *course) {
    char orient;
    float mydir;

    orient = ' ';
    if (strlen(course)) {
        mydir = (float)atof(course);
        if (mydir > 0) {
            if (mydir < (float)( 180+ANGLE_UPDOWN ) )
                orient = 'd';
            if (mydir < (float)( 180-ANGLE_UPDOWN ) )
                orient = 'r';
            if (mydir < (float)ANGLE_UPDOWN || mydir > (float)( 360-ANGLE_UPDOWN) )
                orient = 'u';
        }
    }
    return(orient);
}





void symbol(Widget w, int ghost, char symbol_table, char symbol_id, char symbol_overlay, Pixmap where,
            int mask, long x_offset, long y_offset, char orient) {
    int i;
    int found;
    int nosym;
    int alphanum;


    /* DK7IN: orient  is ' ','l','r','u','d'  for left/right/up/down symbol orientation */
    // if symbol could be rotated, normal symbol orientation in symbols.dat is to the left
    found = nosym = alphanum = -1;

    if (symbol_overlay == '\0' || symbol_overlay == ' ')
        alphanum = 0;          // we don't want an overlay

    for (i=0;(i<symbols_loaded);i++) {
        if (symbol_data[i].active==SYMBOL_ACTIVE) {
            if (symbol_data[i].table == symbol_table && symbol_data[i].symbol == symbol_id)
                if (found==-1) {found=i;}       // index of symbol

            if (symbol_data[i].table=='!' && symbol_data[i].symbol=='#')
                nosym=i;       // index of special symbol (if none available)

            if (symbol_data[i].table=='#' && symbol_data[i].symbol==symbol_overlay)
                alphanum=i;    // index of symbol for character overlay

            if (alphanum != -1 && nosym != -1 && found != -1)
                break;

        }
    }

    if (found == -1) {
        found=nosym;
        if (symbol_table && symbol_id && debug_level & 128)
            printf("No Symbol Yet! %2x:%2x\n", (unsigned int)symbol_table, (unsigned int)symbol_id);
    } else {                    // maybe we want a rotated symbol

// It looks like we originally did not want to rotate the symbol if
// it was ghosted?  Why?  For dead-reckoning we do want it to be
// rotated when ghosted.
//      if (!(orient == ' ' || orient == 'l' || symbol_data[found].orient == ' ' || ghost)) {
        if (!(orient == ' ' || orient == 'l' || symbol_data[found].orient == ' ')) {
            for (i=found;(i<symbols_loaded);i++) {
                if (symbol_data[i].active==SYMBOL_ACTIVE) {
                    if (symbol_data[i].table == symbol_table && symbol_data[i].symbol == symbol_id
                             && symbol_data[i].orient == orient) {
                        found=i;  // index of rotated symbol
                        break;
                    }
                }
            }
        }
    }

    if (mask) {
        if (ghost)
            (void)XSetClipMask(XtDisplay(w),gc,symbol_data[found].pix_mask_old);
        else
            (void)XSetClipMask(XtDisplay(w),gc,symbol_data[found].pix_mask);
    }
    (void)XSetClipOrigin(XtDisplay(w),gc,x_offset,y_offset);
    (void)XCopyArea(XtDisplay(w),symbol_data[found].pix,where,gc,0,0,20,20,x_offset,y_offset);

    if(alphanum>0) {
        if (ghost)
            (void)XSetClipMask(XtDisplay(w),gc,symbol_data[alphanum].pix_mask_old);
        else
            (void)XSetClipMask(XtDisplay(w),gc,symbol_data[alphanum].pix_mask);

        (void)XSetClipOrigin(XtDisplay(w),gc,x_offset,y_offset);
        (void)XCopyArea(XtDisplay(w),symbol_data[alphanum].pix,where,gc,0,0,20,20,x_offset,y_offset); // rot
    }

    (void)XSetClipMask(XtDisplay(w),gc,None);
}





// Speed is in converted units by this point (kph or mph)
void draw_symbol(Widget w, char symbol_table, char symbol_id, char symbol_overlay, long x_long,long y_lat,
                 char *callsign_text, char *alt_text, char *course_text, char *speed_text, char *my_distance,
                 char *my_course, char *wx_temp, char* wx_wind, time_t sec_heard, int temp_show_last_heard,
                 Pixmap where, char orient, char area_type) {
    long x_offset,y_offset;
    int length;
    int ghost;
    int posyl;
    int posyr;

    if ( ((sec_clear+sec_heard)>sec_now()) || show_old_data ) {
        if((x_long+10>=0) && (x_long-10<=129600000l)) {      // 360 deg
            if((y_lat+10>=0) && (y_lat-10<=64800000l)) {     // 180 deg
                if((x_long>x_long_offset) && (x_long<(x_long_offset+(long)(screen_width *scale_x)))) {
                    if((y_lat>y_lat_offset) && (y_lat<(y_lat_offset+(long)(screen_height*scale_y)))) {
                        x_offset=((x_long-x_long_offset)/scale_x)-(10);
                        y_offset=((y_lat -y_lat_offset) /scale_y)-(10);
                        ghost = (int)(((sec_old+sec_heard)) < sec_now());
                        symbol(w,ghost,symbol_table,symbol_id,symbol_overlay,where,1,x_offset,y_offset,orient);

                        posyr = 10;      // align symbols vertically centered to the right
                        if ( (!ghost || show_old_data) && strlen(alt_text)>0)
                            posyr -= 6;
                        if (strlen(callsign_text)>0)
                            posyr -= 6;
                        if ( (!ghost || show_old_data) && strlen(speed_text)>0)
                            posyr -= 6;
                        if ( (!ghost || show_old_data) && strlen(course_text)>0)
                            posyr -= 6;
                        if (area_type == AREA_LINE_RIGHT)
                            posyr += 8;
                        // we may eventually have more adjustments for different types of areas

                        length=(int)strlen(alt_text);
                        if ( (!ghost || show_old_data) && length>0) {
                            x_offset=((x_long-x_long_offset)/scale_x)+12;
                            y_offset=((y_lat -y_lat_offset) /scale_y)+posyr;
                            draw_nice_string(w,where,letter_style,x_offset,y_offset,alt_text,0x08,0x48,length);
                            posyr += 12;
                        }

                        length=(int)strlen(callsign_text);
                        if (length>0) {
                            x_offset=((x_long-x_long_offset)/scale_x)+12;
                            y_offset=((y_lat -y_lat_offset) /scale_y)+posyr;
                            draw_nice_string(w,where,letter_style,x_offset,y_offset,callsign_text,0x08,0x0f,length);
                            posyr += 12;
                        }

                        length=(int)strlen(speed_text);
                        if ( (!ghost || show_old_data) && length>0) {
                            x_offset=((x_long-x_long_offset)/scale_x)+12;
                            y_offset=((y_lat -y_lat_offset) /scale_y)+posyr;
                            draw_nice_string(w,where,letter_style,x_offset,y_offset,speed_text,0x08,0x4a,length);
                            posyr += 12;
                        }

                        length=(int)strlen(course_text);
                        if ( (!ghost || show_old_data) && length>0) {
                            x_offset=((x_long-x_long_offset)/scale_x)+12;
                            y_offset=((y_lat -y_lat_offset) /scale_y)+posyr;
                            draw_nice_string(w,where,letter_style,x_offset,y_offset,course_text,0x08,0x52,length);
                            posyr += 12;
                        }

                        posyl = 10; // distance and direction goes to the left.
                                    // Also minutes last heard.
                        if ( (!ghost || show_old_data) && strlen(my_distance)>0)
                            posyl -= 6;
                        if ( (!ghost || show_old_data) && strlen(my_course)>0)
                            posyl -= 6;
                        if ( (!ghost || show_old_data) && temp_show_last_heard)
                            posyl -= 6;

                        length=(int)strlen(my_distance);
                        if ( (!ghost || show_old_data) && length>0) {
                            x_offset=(((x_long-x_long_offset)/scale_x)-(length*6))-12;
                            y_offset=((y_lat  -y_lat_offset) /scale_y)+posyl;
                            draw_nice_string(w,where,letter_style,x_offset,y_offset,my_distance,0x08,0x0f,length);
                            posyl += 12;
                        }
                        length=(int)strlen(my_course);
                        if ( (!ghost || show_old_data) && length>0) {
                            x_offset=(((x_long-x_long_offset)/scale_x)-(length*6))-12;
                            y_offset=((y_lat  -y_lat_offset) /scale_y)+posyl;
                            draw_nice_string(w,where,letter_style,x_offset,y_offset,my_course,0x08,0x0f,length);
                            posyl += 12;
                        }
                        if ( (!ghost || show_old_data) && temp_show_last_heard) {
                            char age[20];
                            float minutes;
                            float hours;
                            int fgcolor;

                            // Color code the time string based on
                            // time since last heard:
                            //  Green:  0-29 minutes
                            // Yellow: 30-59 minutes
                            //    Red: 60 minutes to 1 day
                            //  White: 1 day or later

                            minutes = (float)( (sec_now() - sec_heard) / 60.0);
                            hours = minutes / 60.0;

                            // Heard from this station within the
                            // last 30 minutes?
                            if (minutes < 30.0) {
                                xastir_snprintf(age,sizeof(age),"%dmin", (int)minutes);
                                fgcolor = 0x52; // green
                            }
                            // 30 to 59 minutes?
                            else if (minutes < 60.0) {
                                xastir_snprintf(age,sizeof(age),"%dmin", (int)minutes);
                                fgcolor = 0x40; // yellow
                            }
                            // 1 hour to 1 day old?
                            else if (hours <= 24.0) {
                                xastir_snprintf(age,sizeof(age),"%.1fhr", hours);
                                fgcolor = 0x4a; // red
                            }
                            // More than a day old
                            else {
                                xastir_snprintf(age,sizeof(age),"%.1fday", hours / 24.0);
                                fgcolor = 0x0f; // white
                            }

                            length = strlen(age);
                            x_offset=(((x_long-x_long_offset)/scale_x)-(length*6))-12;
                            y_offset=((y_lat  -y_lat_offset) /scale_y)+posyl;
                            draw_nice_string(w,where,letter_style,x_offset,y_offset,age,0x08,fgcolor,length);
                            posyl += 12;
                        }
                                                                                                                    
                        if (posyr < posyl)  // weather goes to the bottom, centered horizontally
                            posyr = posyl;
                        if (posyr < 18)
                            posyr = 18;
                                        
                        length=(int)strlen(wx_temp);
                        if ( (!ghost || show_old_data) && length>0) {
                            x_offset=((x_long-x_long_offset)/scale_x)-(length*3);
                            y_offset=((y_lat -y_lat_offset) /scale_y)+posyr;
                            draw_nice_string(w,where,letter_style,x_offset,y_offset,wx_temp,0x08,0x40,length);
                            posyr += 12;
                        }

                        length=(int)strlen(wx_wind);
                        if ( (!ghost || show_old_data) && length>0) {
                            x_offset=((x_long-x_long_offset)/scale_x)-(length*3);
                            y_offset=((y_lat -y_lat_offset) /scale_y)+posyr;
                            draw_nice_string(w,where,letter_style,x_offset,y_offset,wx_wind,0x08,0x40,length);
                        }
                    }
                }
            }
        }
    }
}





/*
 * Looks at the style to determine what color to use.
 * KG4NBB
 */
static int getLineColor(char styleChar) {
    int color;

    switch (styleChar) {
        case 'a':
        case 'b':
        case 'c':
            color = colors[0x0c];   // red
            break;

        case 'd':
        case 'e':
        case 'f':
            color = colors[0x0e];   // yellow
            break;

        case 'g':
        case 'h':
        case 'i':
            color = colors[0x09];   // blue
            break;

        case 'j':
        case 'k':
        case 'l':
            color = colors[0x0a];   // green
            break;

        default:
            color = colors[0x0a];   // green
            break;
    }

    return color;
}





/*
 * Looks at the style to determine what line type to use.
 * KG4NBB
 */
static int getLineStyle(char styleChar) {
    int style;

    switch (styleChar) {
        case 'a':
        case 'd':
        case 'g':
        case 'j':
            style = LineSolid;
            break;
        
        case 'b':
        case 'e':
        case 'h':
        case 'k':
            style = LineOnOffDash;
            break;

        case 'c':
        case 'f':
        case 'i':
        case 'l':
            style = LineDoubleDash;
            break;

        default:
            style = LineSolid;
            break;
    }

    return style;
}





/*
 * Draw the other points associated with the station.
 * KG4NBB
 */
void draw_multipoints(long x_long, long y_lat, int numpoints, long mypoints[][2], char type, char style, time_t sec_heard, Pixmap where) {
    // See if we should draw multipoints for this station. This only happens
    // if there are points to draw, and the object has not been cleared (or 
    // we're supposed to show old data).

    if (numpoints > 0 && (((sec_clear + sec_heard) > sec_now()) || show_old_data )) {
        //long x_offset, y_offset;
        int  i;
        XPoint xpoints[MAX_MULTIPOINTS + 1];

#if 0
        long mostNorth, mostSouth, mostWest, mostEast;

        // Check to see if the object is onscreen.
        // Look for the coordinates that are farthest north, farthest south,
        // farthest west, and farthest east. Then check to see if any of that
        // box is onscreen. If so, proceed with drawing. This is all done in
        // Xastir coordinates.

        mostNorth = mostSouth = y_lat;
        mostWest = mostEast = x_long;

        for (i = 0; i < numpoints; ++i) {
            if (mypoints[i][0] < mostNorth)
                mostNorth = mypoints[i][0];
            if (mypoints[i][0] > mostSouth)
                mostSouth = mypoints[i][0];
            if (mypoints[i][1] < mostWest)
                mostWest = mypoints[i][1];
            if (mypoints[i][1] > mostEast)
                mostEast = mypoints[i][1];
        }
        
        if (onscreen(mostWest, mostEast, mostNorth, mostSouth))
#else

        // See if the station icon is on the screen. If so, draw the associated
        // points. The drawback to this approach is that if the station icon is
        // scrolled off the edge of the display the points aren't drawn even if
        // one or more of them is on the display.

        if((x_long > x_long_offset) && (x_long < (x_long_offset + (long)(screen_width * scale_x)))
            && (y_lat > y_lat_offset) && (y_lat < (y_lat_offset + (long)(screen_height * scale_y)))) 
#endif
        {
            //x_offset = (x_long - x_long_offset) / scale_x;
            //y_offset = (y_lat - y_lat_offset) / scale_y;

            // Convert each of the points from Xastir coordinates to
            // screen coordinates and fill in the xpoints array.

            for (i = 0; i < numpoints; ++i) {
                xpoints[i].x = (mypoints[i][0] - x_long_offset) / scale_x;
                xpoints[i].y = (mypoints[i][1] - y_lat_offset) / scale_y;
                // printf("   %d: %d,%d\n", i, xpoints[i].x, xpoints[i].y);
            }

            // The type parameter determines how the points will be used.
            // After determining the type, use the style parameter to
            // get the color and line style.

            switch (type) {
                case '0':           // closed polygon
                default:
                    // Repeat the first point so the polygon will be closed.

                    xpoints[numpoints].x = xpoints[0].x;
                    xpoints[numpoints].y = xpoints[0].y;

                    // First draw a wider black line.

                    (void)XSetForeground(XtDisplay(da), gc, colors[0x08]);  // black
                    (void)XSetLineAttributes(XtDisplay(da), gc, 4, LineSolid, CapButt, JoinMiter);
                    (void)XDrawLines(XtDisplay(da), where, gc, xpoints, numpoints+1, CoordModeOrigin);

                    // Then draw the appropriate colored line on top of it.

                    (void)XSetForeground(XtDisplay(da), gc, getLineColor(style));
                    (void)XSetLineAttributes(XtDisplay(da), gc, 2, getLineStyle(style), CapButt, JoinMiter);
                    (void)XDrawLines(XtDisplay(da), where, gc, xpoints, numpoints+1, CoordModeOrigin);
                    
                    break;

                case '1':           // line segments
                    (void)XSetForeground(XtDisplay(da), gc, getLineColor(style));
                    (void)XSetLineAttributes(XtDisplay(da), gc, 2, getLineStyle(style), CapButt, JoinMiter);
                    (void)XDrawLines(XtDisplay(da), where, gc, xpoints, numpoints, CoordModeOrigin);
                    
                    break;

                // Other types have yet to be implemented.
            }
        }
    }
}





void Select_symbol_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    int i;

    XtPopdown(shell);

    // Free all 188 symbol pixmaps
    for ( i = 0; i < (126-32)*2; i++ ) {
        (void)XFreePixmap(XtDisplay(appshell),select_icons[i]);
    }

begin_critical_section(&select_symbol_dialog_lock, "draw_symbols.c:Select_symbol_destroy_shell" );
 
    XtDestroyWidget(shell);
    select_symbol_dialog = (Widget)NULL;

end_critical_section(&select_symbol_dialog_lock, "draw_symbols.c:Select_symbol_destroy_shell" );

}





void Select_symbol_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    char table[2];
    char symbol[2];
    long i;

    i = (long)clientData;

    //printf("Selected a symbol: %d\n", clientData);

    if ( (int)clientData > 0) {
        //printf("Symbol is from primary symbol table: /%c\n",(char)i);
        table[0] = '/';
        symbol[0] = (char)i;
    }
    else {
        //printf("Symbol is from secondary symbol table: \\%c\n",(char)(-i));
        table[0] = '\\';
        symbol[0] = (char)(-i);
    }
    table[1] = '\0';
    symbol[1] = '\0';


    if (symbol_change_requested_from == 1) {        // Configure->Station Dialog
        symbol_change_requested_from = 0;
        //printf("Updating Configure->Station Dialog\n");

        XmTextFieldSetString(station_config_group_data,table);
        XmTextFieldSetString(station_config_symbol_data,symbol);
        updateSymbolPictureCallback(widget,clientData,callData);
    }
    else if (symbol_change_requested_from == 2) {   // Create->Object/Item Dialog
        symbol_change_requested_from = 0;
        //printf("Updating Create->Object/Item Dialog\n");

        XmTextFieldSetString(object_group_data,table);
        XmTextFieldSetString(object_symbol_data,symbol);
        updateObjectPictureCallback(widget,clientData,callData);
    }
    else {  // Do nothing.  We shouldn't be here.
        symbol_change_requested_from = 0;
    }

/*
    // Small memory leak in below statement:
    //strcpy(altnet_call, XmTextGetString(altnet_text));
    // Change to:
    temp = XmTextGetString(altnet_text);
    strcpy(altnet_call,temp);
    XtFree(temp);
*/
    Select_symbol_destroy_shell(widget,select_symbol_dialog,callData);
}





void Select_symbol( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget  pane, my_form, my_form2, my_form3, button_cancel,
            frame, frame2, label1, label2, b1;
    int i;
    Atom delw;


    if (!select_symbol_dialog) {


begin_critical_section(&select_symbol_dialog_lock, "draw_symbols.c:Select_symbol" );


        select_symbol_dialog = XtVaCreatePopupShell(langcode("SYMSEL0001"),
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Select_symbol pane",
                xmPanedWindowWidgetClass, 
                select_symbol_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Select_symbol my_form",
                xmFormWidgetClass, 
                pane,
                XmNfractionBase, 5,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        frame = XtVaCreateManagedWidget("Select_symbol frame", 
                xmFrameWidgetClass, 
                my_form,
                XmNtopAttachment,XmATTACH_FORM,
                XmNtopOffset,10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        label1 = XtVaCreateManagedWidget(langcode("SYMSEL0002"),
                xmLabelWidgetClass,
                frame,
                XmNchildType, XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        frame2 = XtVaCreateManagedWidget("Select_symbol frame", 
                xmFrameWidgetClass, 
                my_form,
                XmNtopAttachment,XmATTACH_FORM,
                XmNtopOffset,10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, frame,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        label2 = XtVaCreateManagedWidget(langcode("SYMSEL0003"),
                xmLabelWidgetClass,
                frame2,
                XmNchildType, XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form2 =  XtVaCreateWidget("Select_symbol my_form2",
                xmRowColumnWidgetClass, 
                frame,
                XmNorientation, XmHORIZONTAL,
                XmNpacking, XmPACK_COLUMN,
                XmNnumColumns, 10,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form3 =  XtVaCreateWidget("Select_symbol my_form3",
                xmRowColumnWidgetClass, 
                frame2,
                XmNorientation, XmHORIZONTAL,
                XmNpacking, XmPACK_COLUMN,
                XmNnumColumns, 10,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // Symbols:  33 to 126, for both '/' and '\' tables (94 * 2)
        // 33 = start of icons in ASCII table, 126 = end

        // Draw the primary symbol set
        for ( i = 33; i < 127; i++ ) {

            select_icons[i-33] = XCreatePixmap(XtDisplay(appshell),
                    RootWindowOfScreen(XtScreen(appshell)),
                    20,
                    20,
                    DefaultDepthOfScreen(XtScreen(appshell)));
 
            b1 = XtVaCreateManagedWidget("symbol button",
                    xmPushButtonWidgetClass, 
                    my_form2,
                    XmNlabelType,               XmPIXMAP,
                    XmNlabelPixmap,             select_icons[i-33],
                    XmNnavigationType, XmTAB_GROUP,
                    MY_FOREGROUND_COLOR,
                    MY_BACKGROUND_COLOR,
                    NULL);

            symbol(b1,0,'/',(char)i,' ',select_icons[i-33],0,0,0,' ');  // create icon

            // Here we send back the ascii number of the symbol.  We need to keep it within
            // the range of short int's.
            XtAddCallback(b1, XmNactivateCallback, Select_symbol_change_data, (XtPointer)i );
        }

        // Draw the alternate symbol set
        for ( i = 33+94; i < 127+94; i++ ) {

            select_icons[i-33] = XCreatePixmap(XtDisplay(appshell),
                    RootWindowOfScreen(XtScreen(appshell)),
                    20,
                    20,
                    DefaultDepthOfScreen(XtScreen(appshell)));
 
            b1 = XtVaCreateManagedWidget("symbol button",
                    xmPushButtonWidgetClass, 
                    my_form3,
                    XmNlabelType,               XmPIXMAP,
                    XmNlabelPixmap,             select_icons[i-33],
                    XmNnavigationType, XmTAB_GROUP,
                    MY_FOREGROUND_COLOR,
                    MY_BACKGROUND_COLOR,
                    NULL);

            symbol(b1,0,'\\',(char)i-94,' ',select_icons[i-33],0,0,0,' ');  // create icon

            // Here we send back the ascii number of the symbol negated.  We need to keep it
            // within the range of short int's.
            XtAddCallback(b1, XmNactivateCallback, Select_symbol_change_data, (XtPointer)(-(i-94)) );
        }

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, frame,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(button_cancel, XmNactivateCallback, Select_symbol_destroy_shell, select_symbol_dialog);

        pos_dialog(select_symbol_dialog);

        delw = XmInternAtom(XtDisplay(select_symbol_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(select_symbol_dialog, delw, Select_symbol_destroy_shell, (XtPointer)select_symbol_dialog);
        XtManageChild(my_form3);
        XtManageChild(my_form2);
        XtManageChild(my_form);
        XtManageChild(pane);

        XtPopup(select_symbol_dialog,XtGrabNone);
        fix_dialog_size(select_symbol_dialog);

        // Move focus to the Close button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(select_symbol_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);


end_critical_section(&select_symbol_dialog_lock, "draw_symbols.c:Select_symbol" );


    } else
        (void)XRaiseWindow(XtDisplay(select_symbol_dialog), XtWindow(select_symbol_dialog));
}





// Function to draw a line in the direction of travel.  Use speed in knots to determine the
// flags and barbs to draw along the shaft.  Course is in true
// degrees, in the direction that the wind is coming from.
//
void draw_deadreckoning_features(DataRow *p_station, Pixmap where, Widget w) {
    int my_course = atoi(p_station->course);   // In ° true
    int shaft_length = 0;
    float bearing_radians;
    long off_x, off_y;
    long x_long, y_lat;
    long x, y;
    double diameter;
    long max_x, max_y;
    double range;
    int color = trail_colors[p_station->trail_color];


    x_long = p_station->coord_lon;
    y_lat = p_station->coord_lat;

    range = (double)((sec_now()-p_station->sec_heard)*(1.1508/3600)*(atof(p_station->speed)));

    // Adjust so that it fits our screen angles.  We're off by
    // 90 degrees.
    my_course = (my_course - 90) % 360;

    shaft_length = ( range /
        (scale_x * calc_dscale_x(mid_x_long_offset,mid_y_lat_offset) * 0.0006214) );

    // Draw shaft at proper angle.
    bearing_radians = (my_course/360.0) * 2.0 * M_PI;

    off_y = (long)( shaft_length * sin(bearing_radians) );
    off_x = (long)( shaft_length * cos(bearing_radians) );

    x = (x_long - x_long_offset)/scale_x;
    y = (y_lat - y_lat_offset)/scale_y;

    /* max off screen values */
    max_x = screen_width+800l;
    max_y = screen_height+800l;

    if (show_DR_arc &&
        ((x_long>=0) && (x_long<=129600000l)) &&
	((y_lat>=0) && (y_lat<=64800000l))) {

// Prevents it from being drawn when the symbol is off-screen.
// It'd be better to check for lat/long +/- range to see if it's on the screen.

	if ((x_long>x_long_offset) &&
	    (x_long<(x_long_offset+(long)(screen_width *scale_x))) &&
	    ((y_lat>y_lat_offset) &&
	     (y_lat<(y_lat_offset+(long)(screen_height*scale_y))))) {

	    // Range is in miles.  Bottom term is in
	    // meters before the 0.0006214
	    // multiplication factor which converts it
	    // to miles.  Equation is:  2 * ( range(mi)
	    // / x-distance across window(mi) )
	    diameter = 2.0 * ( range/
			       (scale_x * calc_dscale_x(mid_x_long_offset,mid_y_lat_offset) * 0.0006214 ) );

	    //printf("Range:%f\tDiameter:%f\n",range,diameter);

	    if (diameter > 10.0) {
		int arc_degrees = (sec_now() - p_station->sec_heard) * 90 / (5*60);
		if (arc_degrees > 360) {
		    arc_degrees = 360;
		}

		(void)XSetLineAttributes(XtDisplay(da), gc, 1, LineOnOffDash, CapButt,JoinMiter);
		//(void)XSetForeground(XtDisplay(da),gc,colors[0x0a]);
		//(void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3
		(void)XSetForeground(XtDisplay(da),gc,color);

		(void)XDrawArc(XtDisplay(da),where,gc,
			       (int)(x-(diameter/2)),
			       (int)(y-(diameter/2)),
			       (unsigned int)diameter, (unsigned int)diameter,
			       -64*my_course,
			       64/2*arc_degrees);
		(void)XDrawArc(XtDisplay(da),where,gc,
			       (int)(x-(diameter/2)),
			       (int)(y-(diameter/2)),
			       (unsigned int)diameter, (unsigned int)diameter,
			       -64*my_course,
			       -64/2*arc_degrees);
	    }
        }
    }

    if (show_DR_course) {
        (void)XSetLineAttributes(XtDisplay(da), gc, 0, LineOnOffDash, CapButt,JoinMiter);
        (void)XSetForeground(XtDisplay(da),gc,color); // red3
        (void)XDrawLine(XtDisplay(da),where,gc,
            x,
            y,
            x + off_x,
            y + off_y);
    }

    if (show_DR_symbol) {
        draw_symbol(w,
            p_station->aprs_symbol.aprs_type,
            p_station->aprs_symbol.aprs_symbol,
            p_station->aprs_symbol.special_overlay,
            ((x+off_x)*scale_x)+x_long_offset,
            ((y+off_y)*scale_y)+y_lat_offset,
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            p_station->sec_heard-sec_old,
            0,
            where,
            symbol_orient(p_station->course),
            p_station->aprs_symbol.area_object.type);
    }
}


