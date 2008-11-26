/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2008  The Xastir Group
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
#include <math.h>

#include <Xm/XmAll.h>

#include "xastir.h"
#include "database.h"
#include "draw_symbols.h"
#include "main.h"
#include "util.h"
#include "color.h"
#include "maps.h"

// Must be last include file
#include "leak_detection.h"


extern XmFontList fontlist1;    // Menu/System fontlist


#define ANGLE_UPDOWN 30         /* prefer horizontal cars if less than 45 degrees */

int symbols_loaded = 0;
int symbols_cache[5] = {0,0,0,0,0};
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
    GContext gcontext;
    XFontStruct *xfs_ptr;
    int font_width, font_height;


    if (x > screen_width)  return;
    if (x < 0)             return;
    if (y > screen_height) return;
    if (y < 0)             return;

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

// With a large font, the background rectangle is too small.  Need
// to include the font metrics in this drawing algorithm.

            (void)XSetForeground(XtDisplay(w),gc,colors[0xff]);
            (void)XFillRectangle(XtDisplay(w),where,gc,x-1,(y-10),(length*6)+2,11);
            (void)XSetForeground(XtDisplay(w),gc,colors[bgcolor]);
            (void)XDrawString(XtDisplay(w),where,gc,x+1,y+1,text,length);
            break;

        case 2:
        default:
            // draw white or colored text in a black box

            // With a large font, the background rectangle is too
            // small.  Need to include the font metrics in this
            // drawing algorithm, which we do here.

            gcontext = XGContextFromGC(gc);

            xfs_ptr = XQueryFont(XtDisplay(w), gcontext);


//            font_width = xfs_ptr->max_bounds.width
//                + xfs_ptr->max_bounds.rbearing
//                - xfs_ptr->max_bounds.lbearing;
//            font_width = xfs_ptr->max_bounds.width;
            font_width = (int)((xfs_ptr->max_bounds.width
                + xfs_ptr->max_bounds.width
                + xfs_ptr->max_bounds.width
                + xfs_ptr->min_bounds.width) / 4);

            font_height = xfs_ptr->max_bounds.ascent
                + xfs_ptr->max_bounds.descent;

            if (xfs_ptr) {
                // This leaks memory if the last parameter is a "0"
                XFreeFontInfo(NULL, xfs_ptr, 1);
            }

            // Normal font returns 10 & 13.  Large system font
            // returns 13 & 20 here.
            //fprintf(stderr,
            //    "Font dimemsions:  Width:%d  Height:%d\n",
            //    font_width,
            //    font_height);
 
            (void)XSetForeground( XtDisplay(w),
                gc,
                GetPixelByName(w,"black") );

// Old:
//(void)XFillRectangle(XtDisplay(w),where,gc,x-2,(y-11),(length*6)+3,13);

// New:  This one makes the black rectangle too long for smaller
// fonts.  Perhaps because they are proportional?
            (void)XFillRectangle( XtDisplay(w),
                where,
                gc,
                x-2,                    // X
                y-font_height,          // Y
                (length*font_width)+3,  // width
                font_height+2);         // height

            break;

    }

    // finally draw the text
    (void)XSetForeground(XtDisplay(w),gc,colors[fgcolor]);
    (void)XDrawString(XtDisplay(w),where,gc,x,y,text,length);
}





/* symbol drawing routines */





// Function to draw a line between a WP symbol "\/" and the
// transmitting station.  We pass it the WP symbol.  It does a
// lookup for the transmitting station callsign to get those
// coordinates, then draws a line between the two symbols if
// possible (both on screen).
//
// If the symbol was a Waypoint symbol, "\/", we need to draw a line
// from the transmitting station to the waypoint symbol according to
// the spec.  Take care to not draw the line any longer than needed
// (don't exercise the X11 long-line drawing bug).  According to the
// spec we also need to change the symbol to just a red dot, but
// it's nice having the "WP" above it so we can differentiate it
// from the other red dot symbol.
//
// We should skip drawing the line if the object/item is not being drawn.
// Should we skip it if the origination station isn't being drawn?
// Do we need to add yet another togglebutton to enable/disable this line?
//
// Note that this type of operation, making a relation between two
// symbols, breaks our paradism quite a bit.  Until now all symbols
// have been independent of each other.  Perhaps we should store the
// location of one symbol in the data of the other so that we won't
// have to compare back and forth.  This won't help much if either
// or both symbols are moving.  Probably better to just do a lookup
// of the originating station by callsign through our lists and then
// draw the line between the two coordinates each time.
//
void draw_WP_line(DataRow *p_station,
                  int ambiguity_flag,
                  long ambiguity_coord_lon,
                  long ambiguity_coord_lat,
                  Pixmap where,
                  Widget w) {
    DataRow *transmitting_station = NULL;
    int my_course;
    long x_long, y_lat;
    long x_long2, y_lat2;
    double lat_m;
    long x, y;
    long x2, y2;
    double temp;
    int color = trail_colors[p_station->trail_color];
    float temp_latitude, temp_latitude2;
    float temp_longitude, temp_longitude2;


    // Compute screen position of waypoint symbol
    if (ambiguity_flag) {
        x_long = ambiguity_coord_lon;
        y_lat = ambiguity_coord_lat;
    }
    else {
        x_long = p_station->coord_lon;
        y_lat = p_station->coord_lat;
    }

    // x & y are screen location of waypoint symbol
    x = (x_long - NW_corner_longitude)/scale_x;
    y = (y_lat - NW_corner_latitude)/scale_y;
 
    // Find transmitting station, get it's position.
    // p_station->origin contains the callsign for the transmitting
    // station.  Do a lookup on that callsign through our database
    // to get the position of that station.

    if (!search_station_name(&transmitting_station,p_station->origin,1)) {
        // Can't find call,
        return;
    }

    x_long2 = transmitting_station->coord_lon;
    y_lat2 = transmitting_station->coord_lat;

    // x2 & y2 are screen location of transmitting station
    x2 = (x_long2 - NW_corner_longitude)/scale_x;
    y2 = (y_lat2 - NW_corner_latitude)/scale_y;


/*
    if ((x2 - x) > 0) {
        my_course = (int)( 57.29578
            * atan( (double)((y2-(y*1.0)) / (x2-(x*1.0) ) ) ) );
    }
    else {
        my_course = (int)( 57.29578
            * atan( (double)((y2-(y*1.0)) / (x-(x2*1.0) ) ) ) );
    }
*/

    // Use the mid-latitude formulas for calculating the rhumb line
    // course.  Modified to minimize the number of conversions we
    // need to do.
//    lat_m = (double)( (y_lat + y_lat2) / 2.0 );

    // Convert from Xastir coordinate system
//    lat_m = (double)( -((lat_m - 32400000l) / 360000.0) );

    lat_m = -((y_lat - 32400000l) / 360000.0)
            + -((y_lat2 - 32400000l) / 360000.0);
    lat_m = lat_m / 2.0;

    convert_from_xastir_coordinates(&temp_longitude2,
        &temp_latitude2,
        x_long2,
        y_lat2);

    convert_from_xastir_coordinates(&temp_longitude,
        &temp_latitude,
        x_long,
        y_lat);

    temp = (double)( (temp_longitude2 - temp_longitude)
            / (temp_latitude2 - temp_latitude) );
// Check for divide-by-zero here????

    // Calculate course and convert to degrees
    my_course = (int)( 57.29578 * atan( cos(lat_m) * temp) );

    // The arctan function returns values between -90 and +90.  To
    // obtain the true course we apply the following rules:
    if (temp_latitude2 > temp_latitude
            && temp_longitude2 > temp_longitude) {
        // Do nothing.
    }
    else if (temp_latitude2 < temp_latitude
            && temp_longitude2 > temp_longitude) {
        my_course = 180 - my_course;
    }
    else if (temp_latitude2 < temp_latitude
            && temp_longitude2 < temp_longitude) {
        my_course = 180 + my_course;
    }
    else if (temp_latitude2 > temp_latitude
            && temp_longitude2 < temp_longitude) {
        my_course = 360 - my_course;
    }
    else {
        // ??
        // Do nothing.
    }

//fprintf(stderr,"course:%d\n", my_course);

    // Convert to screen angle
//    my_course = my_course + 90;

    // Compute whether either of them are on-screen.  If so, draw at
    // least part of the line between them.
    (void)XSetLineAttributes(XtDisplay(da), gc, 0, LineOnOffDash, CapButt,JoinMiter);
    (void)XSetForeground(XtDisplay(da),gc,color); // red3

// Check that our parameters are within spec for XDrawLine.  We'll
// stick to 16-bit values here due to warnings on the man-page
// regarding XSegment structs and the protocol only handling
// short's/unsigned short's, just in case.

    (void)XDrawLine(XtDisplay(da),where,gc,
        l16(x),      // int
        l16(y),      // int
        l16(x2),     // int
        l16(y2));    // int
}

 



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
    double a,b;


// Prevents it from being drawn when the symbol is off-screen.
// It'd be better to check for lat/long +/- range to see if it's on the screen.

//    if ((x_long>NW_corner_longitude) && (x_long<SE_corner_longitude)) {

//        if ((y_lat>NW_corner_latitude) && (y_lat<SE_corner_latitude)) {

//            if ((x_long < 0) || (x_long > 129600000l))
//                return;

//            if ((y_lat < 0) || (y_lat > 64800000l))
//                return;

            // Range is in miles.  Bottom term is in meters before the 0.0006214
            // multiplication factor which converts it to miles.
            // Equation is:  2 * ( range(mi) / x-distance across window(mi) )
            diameter = 2.0 * ( range/
                (scale_x * calc_dscale_x(center_longitude,center_latitude) * 0.0006214 ) );

            // If less than 4 pixels across, skip drawing it.
            if (diameter <= 4.0)
                return;

            a = diameter;
            b = diameter / 2;

            //fprintf(stderr,"Range:%f\tDiameter:%f\n",range,diameter);

            (void)XSetLineAttributes(XtDisplay(da), gc, 2, LineSolid, CapButt,JoinMiter);
            //(void)XSetForeground(XtDisplay(da),gc,colors[0x0a]);
            //(void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3
            (void)XSetForeground(XtDisplay(da),gc,color);

// Check that our parameters are within spec for XDrawArc.  Tricky
// 'cuz the XArc struct has short's and unsigned short's, while
// XDrawArc man-page says int's/unsigned int's.  We'll stick to 16-bit
// just to make sure.

            (void)XDrawArc(XtDisplay(da),where,gc,
                l16(((x_long-NW_corner_longitude)/scale_x)-(diameter/2)), // int
                l16(((y_lat-NW_corner_latitude)/scale_y)-(diameter/2)),   // int
                lu16(diameter),         // unsigned int
                lu16(diameter),         // unsigned int
                l16(0),                 // int
                l16(64*360));           // int

// We may need to check for the lat/long being way too far
// off-screen, refusing to draw the circles if so, if and only if we
// get into X11 drawing bugs as-is.


//        }
//    }
}





// range is in centimeters (0 to 65535 representing 0 to 655.35 meters)
// x_long/y_lat are in Xastir lat/lon units
// lat_precision/lon-precision are in 100ths of seconds of lat/lon
//
void draw_precision_rectangle(long x_long,
                              long y_lat,
                              double range, // Not implemented yet
                              unsigned int lat_precision,
                              unsigned int lon_precision,
                              int color,
                              Pixmap where) {

// Prevents it from being drawn when the symbol is off-screen.
// It'd be better to check for lat/long +/- range to see if it's on the screen.

    if ((x_long>NW_corner_longitude) && (x_long<SE_corner_longitude)) {

        if ((y_lat>NW_corner_latitude) && (y_lat<SE_corner_latitude)) {
            long x2, y2;


//            if ((x_long < 0) || (x_long > 129600000l))
//                return;

//            if ((y_lat < 0) || (y_lat > 64800000l))
//                return;

            (void)XSetLineAttributes(XtDisplay(da), gc, 2, LineSolid, CapButt,JoinMiter);
            //(void)XSetForeground(XtDisplay(da),gc,colors[0x0a]);
            //(void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3
            (void)XSetForeground(XtDisplay(da),gc,color);

            if (x_long > 64800000L) {
                // Eastern hemisphere, add X's (go further east)
                x2 = x_long + lon_precision;
            }
            else {
                // Western hemisphere, subtract X's (go further west)
                x2 = x_long - lon_precision;
            }

            if (y_lat > 32400000L) {
                // Southern hemisphere, add Y's (go further north)
                y2 = y_lat + lat_precision;
            }
            else {
                // Northern hemisphere, subtract Y's (go further south)
                y2 = y_lat - lat_precision;
            }

            draw_vector(da, x_long, y_lat, x_long, y2, gc, where, 0); // x_long constant
            draw_vector(da, x_long, y2, x2, y2, gc, where, 0); // y2 constant
            draw_vector(da, x2, y2, x2, y_lat, gc, where, 0); // x2 constant
            draw_vector(da, x2, y_lat, x_long, y_lat, gc, where, 0); // y_lat constant
        }
    }
}





void draw_phg_rng(long x_long, long y_lat, char *phg, time_t sec_heard, Pixmap where) {
    double range, diameter;
    int offx,offy;
    long xx,yy,xx1,yy1,fxx,fyy;
    double tilt;
    double a,b;
    char is_rng;
    char *strp;


    if ( ((sec_old+sec_heard)>sec_now()) || Select_.old_data ) {

// Prevents it from being drawn when the symbol is off-screen.
// It'd be better to check for lat/long +/- range to see if it's on the screen.

//        if ((x_long>NW_corner_longitude) && (x_long<SE_corner_longitude)) {

//            if ((y_lat>NW_corner_latitude) && (y_lat<SE_corner_latitude)) {

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

//                if ((x_long < 0) || (x_long > 129600000l))
//                    return;

//                if ((y_lat < 0) || (y_lat > 64800000l))
//                    return;

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
                    (scale_x * calc_dscale_x(center_longitude,center_latitude) * 0.0006214 ) );

                // If less than 4 pixels across, skip drawing it.
                if (diameter <= 4.0)
                    return;

                //fprintf(stderr,"PHG: %s, Diameter: %f\n", phg, diameter);

                a = diameter;
                b = diameter / 2;

                if (!is_rng) {  // Figure out the directivity, if outside range of 0-8 it's declared to be an omni

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
                    }   // End of switch
                }

                //fprintf(stderr,"PHG=%02f %0.2f %0.2f %0.2f pix %0.2f\n",range,power,height,gain,diameter);

                (void)XSetLineAttributes(XtDisplay(da), gc, 1, LineSolid, CapButt,JoinMiter);

                if ((sec_old+sec_heard)>sec_now())
                    (void)XSetForeground(XtDisplay(da),gc,colors[0x0a]);
                else
                    (void)XSetForeground(XtDisplay(da),gc,colors[0x52]);

                if (is_rng || phg[6]=='0') {    // Draw circl

// Check that our parameters are within spec for XDrawArc.  Tricky
// 'cuz the XArc struct has short's and unsigned short's, while
// XDrawArc man-page says int's/unsigned int's.  We'll stick to 16-bit
// just to make sure.

                    (void)XDrawArc(XtDisplay(da),where,gc,
                        l16(((x_long-NW_corner_longitude)/scale_x)-(diameter/2)), // int
                        l16(((y_lat-NW_corner_latitude)/scale_y)-(diameter/2)),   // int
                        lu16(diameter), // unsigned int
                        lu16(diameter), // unsigned int
                        l16(0),         // int
                        l16(64*360));   // int
                }
                else {    // Draw oval to depict beam heading

                    // If phg[6] != '0' we still draw a circle, but the center
                    // is offset in the indicated direction by 1/3 the radius.
                                
                    // Draw Circle

// Check that our parameters are within spec for XDrawArc.  Tricky
// 'cuz the XArc struct has short's and unsigned short's, while
// XDrawArc man-page says int's/unsigned int's.  We'll stick to 16-bit
// just to make sure.

                    (void)XDrawArc(XtDisplay(da),where,gc,
                        l16(((x_long-NW_corner_longitude)/scale_x)-(diameter/2) - offx),  // int
                        l16(((y_lat-NW_corner_latitude)/scale_y)-(diameter/2) - offy),    // int
                        lu16(diameter), // unsigned int
                        lu16(diameter), // unsigned int
                        l16(0),         // int
                        l16(64*360));   // int
                }
//            }
//        }
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
    double tilt;
    double a,b;


    if ( ((sec_old+sec_heard)>sec_now()) || Select_.old_data ) {


// Prevents it from being drawn when the symbol is off-screen.
// It'd be better to check for lat/long +/- range to see if it's on the screen.

//        if ((x_long>NW_corner_longitude) && (x_long<SE_corner_longitude)) {

//            if ((y_lat>NW_corner_latitude) && (y_lat<SE_corner_latitude)) {

//                if ((x_long < 0) || (x_long > 129600000l))
//                    return;

//                if ((y_lat < 0) || (y_lat > 64800000l))
//                    return;

                xx=0l;
                yy=0l;
                xx1=0l;
                yy1=0l;
                fxx=0l;
                fyy=0l;
                tilt=0.0;
                offx=0;
                offy=0;

                range = shg_range(shgd[3],shgd[4],shgd[5]);

                // Range is in miles.  Bottom term is in meters before the 0.0006214
                // multiplication factor which converts it to miles.
                // Equation is:  2 * ( range(mi) / x-distance across window(mi) )
                //
                diameter = 2.0 * ( range/
                    (scale_x * calc_dscale_x(center_longitude,center_latitude) * 0.0006214 ) );

                // If less than 4 pixels across, skip drawing it.
                if (diameter <= 4.0)
                    return;

                //fprintf(stderr,"PHG: %s, Diameter: %f\n", shgd, diameter);

                a = diameter;
                b = diameter / 2;

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
                //fprintf(stderr,"PHG=%02f %0.2f %0.2f %0.2f pix %0.2f\n",range,power,height,gain,diameter);

                //fprintf(stderr,"scale_y: %u\n",scale_y);

                if (scale_y > 128) { // Don't fill in circle if zoomed in too far (too slow!)
                    (void)XSetLineAttributes(XtDisplay(da), gc_stipple, 1, LineSolid, CapButt,JoinMiter);
                }
                else {
                    (void)XSetLineAttributes(XtDisplay(da), gc_stipple, 8, LineSolid, CapButt,JoinMiter);
                }

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

// Check that our parameters are within spec for XDrawArc.  Tricky
// 'cuz the XArc struct has short's and unsigned short's, while
// XDrawArc man-page says int's/unsigned int's.  We'll stick to 16-bit
// just to make sure.

                (void)XDrawArc(XtDisplay(da),where,gc_stipple,
                    l16(((x_long-NW_corner_longitude)/scale_x)-(diameter/2) - offx),  // int
                    l16(((y_lat-NW_corner_latitude)/scale_y)-(diameter/2) - offy),    // int
                    lu16(diameter), // unsigned int
                    lu16(diameter), // unsigned int
                    l16(0),         // int
                    l16(64*360));   // int

                if (scale_y > 128) { // Don't fill in circle if zoomed in too far (too slow!)

                    while (diameter > 1.0) {
                        diameter = diameter - 1.0;

// Check that our parameters are within spec for XDrawArc.  Tricky
// 'cuz the XArc struct has short's and unsigned short's, while
// XDrawArc man-page says int's/unsigned int's.  We'll stick to 16-bit
// just to make sure.

                        (void)XDrawArc(XtDisplay(da),where,gc_stipple,
                            l16(((x_long-NW_corner_longitude)/scale_x)-(diameter/2) - offx),  // int
                            l16(((y_lat-NW_corner_latitude)/scale_y)-(diameter/2) - offy),    // int
                            lu16(diameter),  // unsigned int
                            lu16(diameter),  // unsigned int
                            l16(0),         // int
                            l16(64*360));   // int
                    }
                }
//            }
//        }
    }
    // Change back to non-stipple for whatever drawing occurs after this
    (void)XSetFillStyle(XtDisplay(da), gc_stipple, FillSolid);
}





// Draw the ALOHA circle
// Identical to draw_pod_circle when this was first written, but separated
// just in case that POD functionality ever changes per the comments in it
void draw_aloha_circle(long x_long, long y_lat, double range, int color, Pixmap where) {
    double diameter;
    double a,b;
    long width, height;


    // Range is in miles.  Bottom term is in meters before the
    // 0.0006214 multiplication factor which converts it to miles.
    // Equation is:  2 * ( range(mi) / x-distance across window(mi) )
    diameter = 2.0 * ( range/
        (scale_x * calc_dscale_x(center_longitude,center_latitude) * 0.0006214 ) );

    // If less than 4 pixels across, skip drawing it.
    if (diameter <= 4.0)
        return;

    a = diameter;
    b = diameter / 2;


    // Check for the center of the circle being off the edge of the
    // earth (that's _our_ station position by the way!).
    //
//    if ((x_long < 0) || (x_long > 129600000l))
//        return;

//        if ((y_lat < 0) || (y_lat > 64800000l))
//            return;

    //fprintf(stderr,"Range:%f\tDiameter:%f\n",range,diameter);

    width = (((x_long-NW_corner_longitude)/scale_x)-(diameter/2));
    height = (((y_lat-NW_corner_latitude)/scale_y)-(diameter/2));

//    if (width < 0 || width > 32767 || height < 0 || height > 32767) {
//        return;
//    }

    (void)XSetLineAttributes(XtDisplay(da), gc, 2, LineSolid, CapButt,JoinMiter);
    //(void)XSetForeground(XtDisplay(da),gc,colors[0x0a]);
    //(void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3
    (void)XSetForeground(XtDisplay(da),gc,color);

// Check that our parameters are within spec for XDrawArc.  Tricky
// 'cuz the XArc struct has short's and unsigned short's, while
// XDrawArc man-page says int's/unsigned int's.  We'll stick to 16-bit
// just to make sure.

    (void)XDrawArc(XtDisplay(da),where,gc,
        l16(width),     // int
        l16(height),    // int
        lu16(diameter), // unsigned int
        lu16(diameter), // unsigned int
        l16(0),         // int
        l16(64*360));   // int
}





static int barb_len;
static int barb_spacing;





// Change barb parameters based on our current zoom level, so the
// barbs don't get too long as we zoom out.
void set_barb_parameters(void) {
    float factor = 1.0;

    // Initial settings
    barb_len = 16;
    barb_spacing = 16;

    // Scale factor
    if      (scale_y > 80000)
        factor = 3.0;
    else if (scale_y > 40000)
        factor = 2.5;
    else if (scale_y > 20000)
        factor = 2.0;
    else if (scale_y > 10000)
        factor = 1.5;

    // Scale them, plus use poor man's rounding
    barb_len = (int)((barb_len / factor) + 0.5);
    barb_spacing = (int)((barb_spacing / factor) + 0.5);;
}





void draw_half_barbs(int *i, int quantity, float bearing_radians, long x, long y, char *course, Pixmap where) {
    float barb_radians = bearing_radians + ( (45/360.0) * 2.0 * M_PI);
    int j;
    long start_x, start_y, off_x, off_y;


    for (j = 0; j < quantity; j++) {
        // Starting point for barb is (*i * barb_spacing) pixels
        // along bearing_radians vector
        *i = *i + barb_spacing;
        off_x = *i * cos(bearing_radians);
        off_y = *i * sin(bearing_radians);
        start_y = y + off_y;
        start_x = x + off_x;

        // Set off in the barb direction now
        off_y = (long)( (barb_len / 2) * sin(barb_radians) );
        off_x = (long)( (barb_len / 2) * cos(barb_radians) );

        (void)XSetLineAttributes(XtDisplay(da), gc, 0, LineSolid, CapButt,JoinMiter);
        (void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3

// Check that our parameters are within spec for XDrawLine.  We'll
// stick to 16-bit values here due to warnings on the man-page
// regarding XSegment structs and the protocol only handling
// short's/unsigned short's, just in case.

        (void)XDrawLine(XtDisplay(da),where,gc,
            l16(start_x),           // int
            l16(start_y),           // int
            l16(start_x + off_x),   // int
            l16(start_y + off_y));  // int
    }
}





void draw_full_barbs(int *i, int quantity, float bearing_radians, long x, long y, char *course, Pixmap where) {
    float barb_radians = bearing_radians + ( (45/360.0) * 2.0 * M_PI);
    int j;
    long start_x, start_y, off_x, off_y;


    for (j = 0; j < quantity; j++) {
        // Starting point for barb is (*i * barb_spacing) pixels
        // along bearing_radians vector
        *i = *i + barb_spacing;
        off_x = *i * cos(bearing_radians);
        off_y = *i * sin(bearing_radians);
        start_y = y + off_y;
        start_x = x + off_x;

        // Set off in the barb direction now
        off_y = (long)( barb_len * sin(barb_radians) );
        off_x = (long)( barb_len * cos(barb_radians) );

        (void)XSetLineAttributes(XtDisplay(da), gc, 0, LineSolid, CapButt,JoinMiter);
        (void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3

// Check that our parameters are within spec for XDrawLine.  We'll
// stick to 16-bit values here due to warnings on the man-page
// regarding XSegment structs and the protocol only handling
// short's/unsigned short's, just in case.

        (void)XDrawLine(XtDisplay(da),where,gc,
            l16(start_x),           // int
            l16(start_y),           // int
            l16(start_x + off_x),   // int
            l16(start_y + off_y));  // int
    }
}





void draw_triangle_flags(int *i, int quantity, float bearing_radians, long x, long y, char *course, Pixmap where) {
    float barb_radians = bearing_radians + ( (45/360.0) * 2.0 * M_PI);
    int j;
    long start_x, start_y, off_x, off_y, off_x2, off_y2;
    XPoint points[3];


    for (j = 0; j < quantity; j++) {
        // Starting point for barb is (*i * barb_spacing) pixels
        // along bearing_radians vector
        *i = *i + barb_spacing;
        off_x = *i * cos(bearing_radians);
        off_y = *i * sin(bearing_radians);
        start_y = y + off_y;
        start_x = x + off_x;

        // Calculate 2nd point along staff
        off_x2 = (barb_spacing/2) * cos(bearing_radians);
        off_y2 = (barb_spacing/2) * sin(bearing_radians);

        // Set off in the barb direction now
        off_y = (long)( barb_len * sin(barb_radians) );
        off_x = (long)( barb_len * cos(barb_radians) );

        (void)XSetLineAttributes(XtDisplay(da), gc, 0, LineSolid, CapButt,JoinMiter);
        (void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3

        points[0].x = start_x;           points[0].y = start_y;
        points[1].x = start_x + off_x;   points[1].y = start_y + off_y;
        points[2].x = start_x + off_x2;  points[2].y = start_y + off_y2;

        // Number of points is always 3 here, so we don't need to
        // check first before calling XFillPolygon().
        (void)XFillPolygon(XtDisplay(da), where, gc, points, 3, Convex, CoordModeOrigin);
    }
}





void draw_square_flags(int *i, int quantity, float bearing_radians, long x, long y, char *course, Pixmap where) {
    float barb_radians = bearing_radians + ( (90/360.0) * 2.0 * M_PI);
    int j;
    long start_x, start_y, off_x, off_y, off_x2, off_y2;
    XPoint points[4];


    for (j = 0; j < quantity; j++) {
        // Starting point for barb is (*i * barb_spacing) pixels
        // along bearing_radians vector
        *i = *i + barb_spacing;
        off_x = *i * cos(bearing_radians);
        off_y = *i * sin(bearing_radians);
        start_y = y + off_y;
        start_x = x + off_x;

        // Calculate 2nd point along staff
        off_x2 = (barb_spacing/2) * cos(bearing_radians);
        off_y2 = (barb_spacing/2) * sin(bearing_radians);

        // Set off in the barb direction now
        off_y = (long)( barb_len * sin(barb_radians) );
        off_x = (long)( barb_len * cos(barb_radians) );

        (void)XSetLineAttributes(XtDisplay(da), gc, 0, LineSolid, CapButt,JoinMiter);
        (void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3

        points[0].x = start_x;                   points[0].y = start_y;
        points[1].x = start_x + off_x;           points[1].y = start_y + off_y;
        points[2].x = start_x + off_x + off_x2;  points[2].y = start_y + off_y + off_y2;
        points[3].x = start_x + off_x2;          points[3].y = start_y + off_y2;

        // Number of points is always 4 here, so we don't need to
        // check first before calling XFillPolygon().
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


// What to do if my_speed is zero?  Blank out any wind barbs
// that were written before?


    // Prevents it from being drawn when the symbol is off-screen.
    // It'd be better to check for lat/long +/- range to see if it's
    // on the screen.

    if ((x_long>NW_corner_longitude) && (x_long<SE_corner_longitude)) {

        if ((y_lat>NW_corner_latitude) && (y_lat<SE_corner_latitude)) {

//            if ((x_long < 0) || (x_long > 129600000l))
//                return;

//            if ((y_lat < 0) || (y_lat > 64800000l))
//                return;

            // Ok to draw wind barb

        }
        else {
            return;
        }
    }
    else {
        return;
    }

    // Set up the constants for our zoom level
    set_barb_parameters();

    // Convert from mph to knots for wind speed.
    my_speed = my_speed * 0.8689607;

    //fprintf(stderr,"mph:%s, knots:%d\n",speed,my_speed);

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

    shaft_length = barb_spacing * (square_flags + triangle_flags + full_barbs
                   + half_barbs + 1);

    // Set a minimum length for the shaft?
    if (shaft_length < 2)
        shaft_length = 2;

    if (debug_level & 128) {
        fprintf(stderr,"Course:%d,\tL:%d,\tsq:%d,\ttr:%d,\tfull:%d,\thalf:%d\n",
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

    x = (x_long - NW_corner_longitude)/scale_x;
    y = (y_lat - NW_corner_latitude)/scale_y;

    (void)XSetLineAttributes(XtDisplay(da), gc, 0, LineSolid, CapButt,JoinMiter);
    (void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3

// Check that our parameters are within spec for XDrawLine.  We'll
// stick to 16-bit values here due to warnings on the man-page
// regarding XSegment structs and the protocol only handling
// short's/unsigned short's, just in case.

    (void)XDrawLine(XtDisplay(da),where,gc,
            l16(x),             // int
            l16(y),             // int
            l16(x + off_x),     // int
            l16(y + off_y));    // int

    // Increment along shaft and draw filled polygons at:
    // "(angle + 45) % 360" degrees to create flags.

    i = barb_spacing;
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
// TODO: Should we draw with XOR for this function?  Would appear on
// most maps that way, and we wouldn't have to worry much about
// color.
//
// TODO: If Q between 1 and 8, shade the entire area to show the
// beam width?
//
//
// Latest:  We ignore the color parameter and draw everything using
// red3.
//
// The distance calculations below use nautical miles.  Here we
// ignore the difference between nautical and statute miles as it
// really doesn't make much difference how long we draw the vectors:
// The angle is what is important here.
//
void draw_bearing(long x_long, long y_lat, char *course,
        char *bearing, char *NRQ, int color,
        time_t sec_heard, Pixmap where) {
    double range = 0;
    double real_bearing = 0.0;
    double real_bearing_min = 0.0;
    double real_bearing_max = 0.0;
    int width = 0;
    long x_long2, x_long3, y_lat2, y_lat3;
    double screen_miles;


    if ( ((sec_old+sec_heard)>sec_now()) || Select_.old_data ) {

        // Check for a zero value for N.  If found, the NRQ value is meaningless
        // and we need to assume some working default values.
        if (NRQ[0] != '0') {

            // "range" as used below is in nautical miles.
            range = (double)( pow(2.0,NRQ[1] - '0') );

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
                case('0'):  // "Useless" beam width according to spec
                default:
                    return; // Exit routine without drawing vectors
                    break;
            }
        }
        else {  // Assume some default values.
            range = 512.0;  // Assume max range of 512 nautical miles
            width = 8;      // Assume 8 degrees for beam width
        }

        // We have the course of the vehicle and the bearing from the
        // vehicle.  Now we need the real bearing.
        //
        if (atoi(course) != 0) {
            real_bearing = atoi(course) + atoi(bearing);
            real_bearing_min = real_bearing + 360.0 - (width/2.0);
            real_bearing_max = real_bearing + (width/2.0);
        }
        else {
            real_bearing = atoi(bearing);
            real_bearing_min = real_bearing + 360.0 - (width/2.0);
            real_bearing_max = real_bearing + (width/2.0);
        }

        while (real_bearing > 360.0)
            real_bearing -= 360.0;

        while (real_bearing_min > 360.0)
            real_bearing_min -= 360.0;

        while (real_bearing_max > 360.0)
            real_bearing_max -= 360.0;

        // want this in nautical miles
        screen_miles = scale_x * calc_dscale_x(center_longitude,center_latitude) 
          * .5400;

        // Shorten range to more closely fit the screen
        if ( range > (3.0 * screen_miles) )
            range = 3.0 * screen_miles;

        // We now have a distance and a bearing for each vector.
        // Compute the end points via dead-reckoning here.  It will give
        // us points between which we can draw a vector and makes the
        // rest of the code much easier.  Need to skip adding 270
        // degrees if we use that method.
        //
        compute_DR_position(x_long, // input (long)
            y_lat,                  // input (long)
            range,                  // input in nautical miles (double)
            real_bearing_min,       // input in ° true (double)
            &x_long2,               // output (*long)
            &y_lat2);               // output (*long)

        compute_DR_position(x_long, // input (long)
            y_lat,                  // input (long)
            range,                  // input in nautical miles (double)
            real_bearing_max,       // input in ° true (double)
            &x_long3,               // output (*long)
            &y_lat3);               // output (*long)

        (void)XSetLineAttributes(XtDisplay(da), gc, 2, LineSolid, CapButt,JoinMiter);
        //(void)XSetForeground(XtDisplay(da),gc,colors[0x0a]);
        (void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3

        draw_vector(da, x_long, y_lat, x_long2, y_lat2, gc, where, 0);
        draw_vector(da, x_long, y_lat, x_long3, y_lat3, gc, where, 0);
    }

    // Change back to non-stipple for whatever drawing occurs after this
//    (void)XSetFillStyle(XtDisplay(da), gc_tint, FillSolid);
}





// TODO:  Pass back the modified x_long/y_lat to the calling routine
// and use the new lat/long to place the symbol.  This will knock
// off the digits on the right that the ambiguity specifies.  We
// then add 1/2 the rectangle offsets in order to get the symbol
// placed in the middle of the rectangle.
//
void draw_ambiguity(long x_long, long y_lat, char amb, long *amb_x_long, long *amb_y_lat, time_t sec_heard, Pixmap where) {
    unsigned long left, right, top, bottom;
    long offset_lat, offset_long;
    int scale_limit;


    // Assign these first in case we do a sudden return from the
    // function.
    *amb_x_long = x_long;
    *amb_y_lat = y_lat;

//    if ((x_long < 0) || (x_long > 129600000l))
//        return;

//    if ((y_lat < 0) || (y_lat > 64800000l))
//        return;

    switch (amb) {
    case 1: // +- 1/10th minute
        offset_lat = offset_long = 600;
        scale_limit = 256;

        // Truncate digits off the right
        x_long = (long)(x_long / 600);
        x_long = x_long * 600;
        y_lat = (long)(y_lat / 600);
        y_lat = y_lat * 600;
        break;

    case 2: // +- 1 minute
        offset_lat = offset_long = 6000;
        scale_limit = 2048;

        // Truncate digits off the right
        x_long = (long)(x_long / 6000);
        x_long = x_long * 6000;
        y_lat = (long)(y_lat / 6000);
        y_lat = y_lat * 6000;
        break;

    case 3: // +- 10 minutes
        offset_lat = offset_long = 60000;
        scale_limit = 16384;

        // Truncate digits off the right
        x_long = (long)(x_long / 60000);
        x_long = x_long * 60000;
        y_lat = (long)(y_lat / 60000);
        y_lat = y_lat * 60000;
        break;

    case 4: // +- 1 degree
        offset_lat = offset_long = 360000;
        scale_limit = 65536;

        // Truncate digits off the right
        x_long = (long)(x_long / 360000);
        x_long = x_long * 360000;
        y_lat = (long)(y_lat / 360000);
        y_lat = y_lat * 360000;
        break;

// TODO:  The last two cases need fixing up like the above

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

    // Re-assign them here as they should have been truncated on the
    // right by the above code.  We'll use these new values
    // to draw the symbols and the other associated symbol data
    // (external to this function).
    //
    *amb_x_long = x_long + (offset_long/2);
    *amb_y_lat = y_lat + (offset_lat / 2);


    if (scale_y > scale_limit) {
        // Ambiguity box will be smaller than smallest symbol so
        // don't draw it.
//fprintf(stderr,"scale_y > scale_limit\n");
        return;
    }

    if ( ((sec_old+sec_heard)<=sec_now()) && !Select_.old_data ) {
        return;
    }

    left   = x_long;
    top    = y_lat;
    right  = x_long + offset_long;
    bottom = y_lat  + offset_lat;


    (void)XSetForeground(XtDisplay(da), gc, colors[0x08]);

    // Draw rectangle (unfilled) plus vectors from symbol to
    // corners.

    (void)XSetLineAttributes(XtDisplay(da), gc,
        2, LineOnOffDash, CapButt,JoinMiter);

    // Top line of rectangle
    draw_vector(da,left,top,right,top,gc,pixmap_final, 0);

    // Bottom line of rectangle
    draw_vector(da,left,bottom,right,bottom,gc,pixmap_final, 1);
        
    // Left line of rectangle
    draw_vector(da,left,top,left,bottom,gc,pixmap_final, 1);
 
    // Right line of rectangle
    draw_vector(da,right,top,right,bottom,gc,pixmap_final, 1);

    // Diagonal lines 
    draw_vector(da,left,top,right,bottom,gc,pixmap_final, 1);
    draw_vector(da,right,top,left,bottom,gc,pixmap_final, 1);
}





// Function which specifies whether any part of a bounding box fits
// on the screen, using screen coordinates as inputs.
//
static __inline__ int onscreen(long left, long right, long top, long bottom) {
    // This checks to see if any part of a box is on the screen.
    if (left > screen_width || right < 0 || top > screen_height || bottom < 0)
        return 0;
    else
        return 1;
}





// According to the spec, the lat/long point is the upper left
// corner of the object, and the offsets are down and to the right
// (except for one line type where it's down and to the left).  This
// doesn't appear to be the case in dos/winAPRS.  Matching what they
// do:
//
// Type 0 Circle:    Tie = center, offsets = vert/horiz. sizes.
// Type 1 Line:      Tie = bottom right, offsets = left and up.
// Type 2 Ellipse:   Tie = center, offsets = vert/horiz. sizes.
// Type 3 Triangle:  Tie = bottom right, offsets = height/width.
// Type 4 Rectangle: Tie = lower right, offsets = left and up.
// Type 5 Circle:    Tie = center, offsets = vert/horiz. sizes.
// Type 6 Line:      Tie = bottom left, offsets = right and up.
// Type 7 Ellipse:   Tie = center, offsets = vert/horiz. sizes.
// Type 8 Triangle:  Tie = bottom right, offsets = height/width.
// Type 9 Rectangle: Tie = lower right, offsets = left and up.
//
// Exceptions to this are the triangle, ellipse, and circle.  The
// ellipse and circle have the lat/long as the center point.  The
// triangle is an isosceles triangle with the lat/long point being
// the bottom right and the bottom of the triangle being horizontal.
//
void draw_area(long x_long, long y_lat, char type, char color,
               char sqrt_lat_off, char sqrt_lon_off, unsigned int width, time_t sec_heard, Pixmap where) {
    long left, top, right, bottom, xoff, yoff;
    int  c;
    XPoint points[4];


//    if ((x_long < 0) || (x_long > 129600000l) ||
//        (y_lat  < 0) || (y_lat  > 64800000l))
//        return;

    xoff = 360000.0 / 1500.0 * (sqrt_lon_off * sqrt_lon_off) / scale_x;
    yoff = 360000.0 / 1500.0 * (sqrt_lat_off * sqrt_lat_off) / scale_y;

    right  = (x_long - NW_corner_longitude) / scale_x;
    bottom = (y_lat  - NW_corner_latitude)  / scale_y;
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
        if (onscreen(left, right, top, bottom)) {

// Check that our parameters are within spec for XDrawRectangle
// Tricky 'cuz the XRectangle struct has short's and unsigned short's,
// while XDrawRectangle man-page says int's/unsigned int's.  We'll
// stick to 16-bit just to make sure.

            (void)XDrawRectangle(XtDisplay(da), where, gc,
                 l16(left),     // int
                 l16(top),      // int
                 lu16(xoff),    // unsigned int
                 lu16(yoff));   // unsigned int
        }
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
    case AREA_OPEN_ELLIPSE:
        right  += xoff;
        bottom += yoff;
        if (onscreen(left, right, top, bottom)) {

// Check that our parameters are within spec for XDrawArc.  Tricky
// 'cuz the XArc struct has short's and unsigned short's, while
// XDrawArc man-page says int's/unsigned int's.  We'll stick to 16-bit
// just to make sure.

            (void)XDrawArc(XtDisplay(da), where, gc,
                 l16(left),         // int
                 l16(top),          // int
                 lu16(2*xoff),      // unsigned int
                 lu16(2*yoff),      // unsigned int
                 l16(0),            // int
                 l16(64 * 360));    // int
        }
        break;
    case AREA_FILLED_CIRCLE:
    case AREA_FILLED_ELLIPSE:
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
// Check for divide-by-zero here???

            int conv_width = width/(scale_x*calc_dscale_x(center_longitude,center_latitude)*0.0006214);
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

                // Number of points is always 4 here, so we don't
                // need to check first before calling
                // XFillPolygon().
                (void)XFillPolygon(XtDisplay(da), where, gc, points, 4, Convex, CoordModeOrigin);
            }
        }
        if (onscreen(left, right, top, bottom)) {
            (void)XSetFillStyle(XtDisplay(da), gc, FillSolid);

// Check that our parameters are within spec for XDrawLine.  We'll
// stick to 16-bit values here due to warnings on the man-page
// regarding XSegment structs and the protocol only handling
// short's/unsigned short's, just in case.

            (void)XDrawLine(XtDisplay(da), where, gc,
                l16(left),      // int
                l16(bottom),    // int
                l16(right),     // int
                l16(top));      // int
        }
        break;
    case AREA_LINE_LEFT:
        if (width > 0) {
            double angle = atan((float)xoff/(float)yoff);
// Check for divide-by-zero here???

            int conv_width = width/(scale_x*calc_dscale_x(center_longitude,center_latitude)*0.0006214);
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

                // Number of points is always 4 here, so we don't
                // need to check first before calling
                // XFillPolygon().
                (void)XFillPolygon(XtDisplay(da), where, gc, points, 4, Convex, CoordModeOrigin);
            }
        }
        if (onscreen(left, right, top, bottom)) {
            (void)XSetFillStyle(XtDisplay(da), gc, FillSolid);

// Check that our parameters are within spec for XDrawLine.  We'll
// stick to 16-bit values here due to warnings on the man-page
// regarding XSegment structs and the protocol only handling
// short's/unsigned short's, just in case.

            (void)XDrawLine(XtDisplay(da), where, gc,
                l16(right),     // int
                l16(bottom),    // int
                l16(left),      // int
                l16(top));      // int
        }
        break;
    case AREA_OPEN_TRIANGLE:
        left -= xoff;
        points[0].x = l16(right);     points[0].y = l16(bottom);
        points[1].x = l16(left+xoff); points[1].y = l16(top);
        points[2].x = l16(left);      points[2].y = l16(bottom);
        points[3].x = l16(right);     points[3].y = l16(bottom);
        if (onscreen(left, right, top, bottom)) {

// Check that our parameters are within spec for XDrawLines.  We'll
// stick to 16-bit values here due to warnings on the man-page
// regarding XSegment structs and the protocol only handling
// short's/unsigned short's, just in case.

            (void)XDrawLines(XtDisplay(da), where, gc,
                points,             // XPoint *
                l16(4),             // int
                CoordModeOrigin);   // int
        }
        break;
    case AREA_FILLED_TRIANGLE:
        left -= xoff;
        points[0].x = l16(right);     points[0].y = l16(bottom);
        points[1].x = l16(left+xoff); points[1].y = l16(top);
        points[2].x = l16(left);      points[2].y = l16(bottom);
        if (onscreen(left, right, top, bottom)) {
            (void)XSetFillStyle(XtDisplay(da), gc, FillStippled);

            // Number of points is always 3 here, so we don't need
            // to check first before calling XFillPolygon().
            (void)XFillPolygon(XtDisplay(da), where, gc, points, 3, Convex, CoordModeOrigin);
        }
        break;
    default:
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
void read_symbol_from_file(FILE *f, char *pixels, char table_char) {
    int x,y;                
    int color;
    char line[100];
    char pixels_copy[400];
    char *p,*q;
    unsigned char a, b, c;

    for (y=0;y<20;y++) {
        (void)get_line(f,line,100);
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

    // Create outline on icons, if needed
    // Do not change the overlays and "number" tables
    if((icon_outline_style != 0) && (table_char != '~') && (table_char != '#'))
    {
        switch(icon_outline_style) {
        case 1:  color = 0x51; // Black
                 break;
        //case 2:  color = 0x43; // Grey 80%
        case 2:  color = 0x4e; // Grey 52%
                 break;
        case 3:  color = 0x4d; // White
                 break;
        default: color = 0xff; // Transparent
                 break;
        }

        p = pixels;
        q = &pixels_copy[0];

        for (y=0;y<20;y++) {
            for (x=0;x<20;x++) {
                *q = *p; // copy current color

                // If transparent see if the pixel is on the edge
                if(*q == (char) 0xff)
                {
                    //check if left or right is none transparent
                    b = c = 0xff;

                    // left (left only possible if x > 0)
                    if(x > 0)
                        b = p[-1];
                    // right (right only possible if x < 19)
                    if(x < 19)
                        c = p[+1];

                    // if non-transparent color is found change pixel
                    // to outline color
                    if((b != (unsigned char) 0xff)
                            || (c != (unsigned char) 0xff)) {
                        // change to icon outline color
                        *q = color;
                    }

                    if((y > 0) && (*q == (char) 0xff)) {
                        //check if left-up, up or right-up is none transparent
                        //"up" only possible if y > 0
                        a = b = c = 0xff;

                        // left-up (left only possible if x > 0)
                        if(x > 0)
                            a = p[-21];
                        // up
                        b = p[-20];
                        // right-up (right only possible if x < 19)
                        if(x < 19)
                            c = p[-19];

                        // if non-transparent color is found change pixel
                        // to outline color
                        if((a != (unsigned char) 0xff)
                                || (b != (unsigned char) 0xff)
                                || (c != (unsigned char) 0xff)) {
                            // change to icon outline color
                            *q = color;
                        }
                    }

                    if((y < 19) && (*q == (char) 0xff)) {
                        //check if left-down, down or right-down is none transparent
                        //"down" only possible if y < 19
                        a = b = c = 0xff;

                        // left-down (left only possible if x > 0)
                        if(x > 0)
                            a = p[+19];
                        // down
                        b = p[+20];
                        // right-down (right only possible if x < 19)
                        if(x < 19)
                            c = p[+21];

                        // if non-transparent color is found change pixel
                        // to outline color
                        if((a != (unsigned char) 0xff)
                                || (b != (unsigned char) 0xff)
                                || (c != (unsigned char) 0xff)) {
                            // change to icon outline color
                            *q = color;
                        }
                    }
                }

                p++;
                q++;
            }
        }
        memcpy(pixels, pixels_copy, 400);
    }
}





/* read in symbol table */
void load_pixmap_symbol_file(char *filename, int reloading) {
    FILE *f;
    char filen[500];
    char line[100];
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
            (void)get_line(f,line,100);
            if (strncasecmp("TABLE ",line,6)==0) {
                table_char=line[6];
                /*fprintf(stderr,"TABLE %c\n",table_char);*/
            } else {
                if (strncasecmp("DONE",line,4)==0) {
                    done=1;
                    /*fprintf(stderr,"DONE\n");*/
                } else {
                    if (strncasecmp("APRS ",line,5)==0) {
                        symbol_char=line[5];
                        if (strlen(line)>=20 && line[19] == 'l')     // symbol with orientation ?
                            orient = 'l';   // should be 'l' for left
                        else
                            orient = ' ';
                        read_symbol_from_file(f, pixels, table_char);                      // read pixels for one symbol
                        insert_symbol(table_char,symbol_char,pixels,270,orient,reloading); // always have normal orientation
                        if (orient == 'l') {
                            insert_symbol(table_char,symbol_char,pixels, 0,'u',reloading); // create other orientations
                            insert_symbol(table_char,symbol_char,pixels, 90,'r',reloading);
                            insert_symbol(table_char,symbol_char,pixels,180,'d',reloading);
                        }
                    }
                }
            }
        }
    } else {
        fprintf(stderr,"Error opening symbol file %s\n",filen);
        popup_message("Error opening symbol file","Error opening symbol file");
    }

    if (f != NULL)
        (void)fclose(f);
}





// add a symbol to the end of the symbol table.
//
// Here we actually draw the pixels into the SymbolData struct,
// which contains separate Pixmap's for the icon, the transparent
// background, and the ghost image.
//
void insert_symbol(char table, char symbol, char *pixel, int deg, char orient, int reloading) {
    int x,y,idx,old_next,color,last_color,last_gc2;

    if (symbols_loaded < MAX_SYMBOLS) {
        // first time loading, -> create pixmap...        
        // when reloading -> reuse already created pixmaps...
        if(reloading == 0) {
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
        }

        old_next=0;
        last_color = -1;    // Something bogus
        last_gc2 = -1;      // Also bogus

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

// Change to new color only when necessary.  We use two different
// GC's here, one for the main icon pixmap, and one for the symbol
// mask and ghost layer.


                // DK7IN: is (da) correct or should this be (appshell) ?
                if (color != last_color) {
                    (void)XSetForeground(XtDisplay(da),gc,colors[color]);
                    last_color = color;
                }

// Check that our parameters are within spec for XDrawPoint.  Tricky
// 'cuz the XPoint struct uses short's, while XDrawPoint manpage
// specifies int's.  We'll stick to 16-bit numbers just to make
// sure.

                (void)XDrawPoint(XtDisplay(da),
                    symbol_data[symbols_loaded].pix,
                    gc,
                    l16(x),     // int
                    l16(y));    // int
                // DK7IN


                // Create symbol mask
                if (color != 0xff) {
                    if (last_gc2 != 1) {
                        (void)XSetForeground(XtDisplay(appshell),gc2,1);  // active bit
                        last_gc2 = 1;
                    }
                }
                else {
                    if (last_gc2 != 0) {
                        (void)XSetForeground(XtDisplay(appshell),gc2,0);  // transparent.
                        last_gc2 = 0;
                    }
                }

// Check that our parameters are within spec for XDrawPoint.  Tricky
// 'cuz the XPoint struct uses short's, while XDrawPoint manpage
// specifies int's.  We'll stick to 16-bit numbers just to make
// sure.

                (void)XDrawPoint(XtDisplay(appshell),
                    symbol_data[symbols_loaded].pix_mask,
                    gc2,
                    l16(x),     // int
                    l16(y));    // int


                // Create ghost symbol mask by setting every 2nd bit
                // to transparent
                old_next++;
                if (old_next>1) {
                    old_next=0;
                    if (last_gc2 != 0) {
                        (void)XSetForeground(XtDisplay(appshell),gc2,0);
                        last_gc2 = 0;
                    }
                }

// Check that our parameters are within spec for XDrawPoint.  Tricky
// 'cuz the XPoint struct uses short's, while XDrawPoint manpage
// specifies int's.  We'll stick to 16-bit numbers just to make
// sure.

                (void)XDrawPoint(XtDisplay(appshell),
                    symbol_data[symbols_loaded].pix_mask_old,
                    gc2,
                    l16(x),     // int
                    l16(y));    // int
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





// Storage for an index into the symbol table that we may need
// later.
int nosym_index = -1;


// Look through our symbol table for a match.
//
void symbol(Widget w, int ghost, char symbol_table, char symbol_id, char symbol_overlay, Pixmap where,
            int mask, long x_offset, long y_offset, char orient) {
    int i;
    int found;
    int alphanum_index = -1;


    if (x_offset > screen_width)  return;
    if (x_offset < 0)             return;
    if (y_offset > screen_height) return;
    if (y_offset < 0)             return;

    /* DK7IN: orient  is ' ','l','r','u','d'  for left/right/up/down symbol orientation */
    // if symbol could be rotated, normal symbol orientation in symbols.dat is to the left


    // Find the nosymbol index if we haven't filled it in by now.
    // This "for" loop should get run only once during Xastir's
    // entire runtime, so it shouldn't contribute much to CPU
    // loading.
    if (nosym_index == -1) {
        for ( i = 0; i < symbols_loaded; i++ ) {
            if (symbol_data[i].active == SYMBOL_ACTIVE) {
                if (symbol_data[i].table == '!'
                        && symbol_data[i].symbol == '#') {
                    nosym_index = i;       // index of special symbol (if none available)
                    break;
                }
            }
        }
    }


    // Handle the overlay character.  The "for" loop below gets run
    // once every time we encounter an overlay character, which
    // isn't all that often.
    if (symbol_overlay == '\0' || symbol_overlay == ' ') {
        alphanum_index = 0; // we don't want an overlay
    }
    else {  // Find the overlay character index
        for ( i = 0; i < symbols_loaded; i++ ) {
            if (symbol_data[i].active == SYMBOL_ACTIVE) {
                if (symbol_data[i].table == '#'
                        && symbol_data[i].symbol == symbol_overlay) {
                    alphanum_index = i; // index of symbol for character overlay
                    break;
                }
            }
        }
    }


    found = -1;

    // Check last few symbols we used to see if we can shortcut
    // looking through the entire index.  The symbols array really
    // should be turned into a hash to save time.  Basically we've
    // implemented a very short cache here, but it keeps us from
    // looking through the entire symbol array sometimes.
    //
    for ( i = 0; i < 5; i++ ) {
//fprintf(stderr,"Checking symbol cache\n");
        if (symbol_data[symbols_cache[i]].table == symbol_table
                && symbol_data[symbols_cache[i]].symbol == symbol_id) {
            // We found the matching symbol in the cache
            found = symbols_cache[i];  // index of symbol
//fprintf(stderr,"Symbol cache hit:%d\n",found);
            break;
        }
    }

    if (found == -1) {  // Not found in symbols cache

        for ( i = 0; i < symbols_loaded; i++ ) {
            if (symbol_data[i].active == SYMBOL_ACTIVE) {
                if (symbol_data[i].table == symbol_table
                        && symbol_data[i].symbol == symbol_id) {
                    // We found the matching symbol
                    found = i;  // index of symbol

                    // Save newly found symbol in cache, shift other
                    // cache entries down by one so that newest is
                    // at the beginning for the cache search.
//fprintf(stderr,"Saving in cache\n");
                    symbols_cache[4] = symbols_cache[3];
                    symbols_cache[3] = symbols_cache[2];
                    symbols_cache[2] = symbols_cache[1];
                    symbols_cache[1] = symbols_cache[0];
                    symbols_cache[0] = i;

                    break;
                }
            }
        }
    }

    if (found == -1) {  // Didn't find a matching symbol
        found = nosym_index;
        if (symbol_table && symbol_id && debug_level & 128)
            fprintf(stderr,"No Symbol Yet! %2x:%2x\n", (unsigned int)symbol_table, (unsigned int)symbol_id);
    } else {                    // maybe we want a rotated symbol


// It looks like we originally did not want to rotate the symbol if
// it was ghosted?  Why?  For dead-reckoning we do want it to be
// rotated when ghosted.
//      if (!(orient == ' ' || orient == 'l' || symbol_data[found].orient == ' ' || ghost)) {
        if (!(orient == ' ' || orient == 'l' || symbol_data[found].orient == ' ')) {
            for (i = found; i < symbols_loaded; i++) {
                if (symbol_data[i].active == SYMBOL_ACTIVE) {
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


    if(alphanum_index > 0) {
        if (ghost)
            (void)XSetClipMask(XtDisplay(w),gc,symbol_data[alphanum_index].pix_mask_old);
        else
            (void)XSetClipMask(XtDisplay(w),gc,symbol_data[alphanum_index].pix_mask);

        (void)XSetClipOrigin(XtDisplay(w),gc,x_offset,y_offset);
        (void)XCopyArea(XtDisplay(w),symbol_data[alphanum_index].pix,where,gc,0,0,20,20,x_offset,y_offset); // rot
    }

    (void)XSetClipMask(XtDisplay(w),gc,None);
}





// Speed is in converted units by this point (kph or mph)
void draw_symbol(Widget w, char symbol_table, char symbol_id, char symbol_overlay,
        long x_long,long y_lat, char *callsign_text, char *alt_text, char *course_text,
        char *speed_text, char *my_distance, char *my_course, char *wx_temp,
        char* wx_wind, time_t sec_heard, int temp_show_last_heard, Pixmap where,
        char orient, char area_type, char *signpost, int bump_count) {

    long x_offset,y_offset;
    int length;
    int ghost;
    int posyl;
    int posyr;


    if ((x_long>NW_corner_longitude) && (x_long<SE_corner_longitude)) {

        if ((y_lat>NW_corner_latitude) && (y_lat<SE_corner_latitude)) {

//            if ((x_long+10 < 0) || (x_long-10 > 129600000l))  // 360 deg
//                return;

//            if ((y_lat+10 < 0) || (y_lat-10 > 64800000l))     // 180 deg
//                return;

            x_offset=((x_long-NW_corner_longitude)/scale_x)-(10);
            y_offset=((y_lat -NW_corner_latitude) /scale_y)-(10);
            ghost = (int)(((sec_old+sec_heard)) < sec_now());

            if (bump_count)
                currently_selected_stations++;

            if (Display_.symbol)
                symbol(w,ghost,symbol_table,symbol_id,symbol_overlay,where,1,x_offset,y_offset,orient);

            posyr = 10;      // align symbols vertically centered to the right
            if ( (!ghost || Select_.old_data) && strlen(alt_text)>0)
                posyr -= 7;
            if (strlen(callsign_text)>0)
                posyr -= 7;
            if ( (!ghost || Select_.old_data) && strlen(speed_text)>0)
                posyr -= 7;
            if ( (!ghost || Select_.old_data) && strlen(course_text)>0)
                posyr -= 7;
            if (area_type == AREA_LINE_RIGHT)
                posyr += 9;
            if (signpost[0] != '\0')    // Signpost data?
                posyr -=7;
            // we may eventually have more adjustments for different types of areas

            length=(int)strlen(alt_text);
            if ( (!ghost || Select_.old_data) && length>0) {
                x_offset=((x_long-NW_corner_longitude)/scale_x)+12;
                y_offset=((y_lat -NW_corner_latitude) /scale_y)+posyr;
                draw_nice_string(w,where,letter_style,x_offset,y_offset,alt_text,0x08,0x48,length);
                posyr += 13;
            }

            length=(int)strlen(callsign_text);
            if (length>0) {
                x_offset=((x_long-NW_corner_longitude)/scale_x)+12;
                y_offset=((y_lat -NW_corner_latitude) /scale_y)+posyr;
                draw_nice_string(w,where,letter_style,x_offset,y_offset,callsign_text,0x08,0x0f,length);
                posyr += 13;
            }

            length=(int)strlen(speed_text);
            if ( (!ghost || Select_.old_data) && length>0) {
                x_offset=((x_long-NW_corner_longitude)/scale_x)+12;
                y_offset=((y_lat -NW_corner_latitude) /scale_y)+posyr;
                draw_nice_string(w,where,letter_style,x_offset,y_offset,speed_text,0x08,0x4a,length);
                posyr += 13;
            }

            length=(int)strlen(course_text);
            if ( (!ghost || Select_.old_data) && length>0) {
                x_offset=((x_long-NW_corner_longitude)/scale_x)+12;
                y_offset=((y_lat -NW_corner_latitude) /scale_y)+posyr;
                draw_nice_string(w,where,letter_style,x_offset,y_offset,course_text,0x08,0x52,length);
                posyr += 13;
            }
 
            length=(int)strlen(signpost);   // Make it white like callsign?
            if ( (!ghost || Select_.old_data) && length>0) {
                x_offset=((x_long-NW_corner_longitude)/scale_x)+12;
                y_offset=((y_lat -NW_corner_latitude) /scale_y)+posyr;
                draw_nice_string(w,where,letter_style,x_offset,y_offset,signpost,0x08,0x0f,length);
                posyr += 13;
            }
 
            posyl = 10; // distance and direction goes to the left.
                                // Also minutes last heard.
            if ( (!ghost || Select_.old_data) && strlen(my_distance)>0)
                posyl -= 7;
            if ( (!ghost || Select_.old_data) && strlen(my_course)>0)
                posyl -= 7;
            if ( (!ghost || Select_.old_data) && temp_show_last_heard)
                posyl -= 7;

            length=(int)strlen(my_distance);
            if ( (!ghost || Select_.old_data) && length>0) {
                x_offset=(((x_long-NW_corner_longitude)/scale_x)-(length*6))-12;
                y_offset=((y_lat  -NW_corner_latitude) /scale_y)+posyl;
                draw_nice_string(w,where,letter_style,x_offset,y_offset,my_distance,0x08,0x0f,length);
                posyl += 13;
            }
            length=(int)strlen(my_course);
            if ( (!ghost || Select_.old_data) && length>0) {
                x_offset=(((x_long-NW_corner_longitude)/scale_x)-(length*6))-12;
                y_offset=((y_lat  -NW_corner_latitude) /scale_y)+posyl;
                draw_nice_string(w,where,letter_style,x_offset,y_offset,my_course,0x08,0x0f,length);
                posyl += 13;
            }
            if ( (!ghost || Select_.old_data) && temp_show_last_heard) {
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
                    xastir_snprintf(age,
                        sizeof(age),
                        "%d%s",
                        (int)minutes,
                        langcode("UNIOP00034"));    // min
                    fgcolor = 0x52; // green
                }
                // 30 to 59 minutes?
                else if (minutes < 60.0) {
                    xastir_snprintf(age,
                        sizeof(age),
                        "%d%s",
                        (int)minutes,
                        langcode("UNIOP00034"));    // min
                    fgcolor = 0x40; // yellow
                }
                // 1 hour to 1 day old?
                else if (hours <= 24.0) {
                    xastir_snprintf(age,
                        sizeof(age),
                        "%.1f%s",
                        hours,
                        langcode("UNIOP00035"));    // hr
                    fgcolor = 0x4a; // red
                }
                // More than a day old
                else {
                    xastir_snprintf(age,
                        sizeof(age),
                        "%.1f%s",
                        hours / 24.0,
                        langcode("UNIOP00036"));    // day
                    fgcolor = 0x0f; // white
                }

                length = strlen(age);
                x_offset=(((x_long-NW_corner_longitude)/scale_x)-(length*6))-12;
                y_offset=((y_lat  -NW_corner_latitude) /scale_y)+posyl;
                draw_nice_string(w,where,letter_style,x_offset,y_offset,age,0x08,fgcolor,length);
                posyl += 13;
            }

            if (posyr < posyl)  // weather goes to the bottom, centered horizontally
                posyr = posyl;
            if (posyr < 18)
                posyr = 18;

            length=(int)strlen(wx_temp);
            if ( (!ghost || Select_.old_data) && length>0) {
                x_offset=((x_long-NW_corner_longitude)/scale_x)-(length*3);
                y_offset=((y_lat -NW_corner_latitude) /scale_y)+posyr;
                draw_nice_string(w,where,letter_style,x_offset,y_offset,wx_temp,0x08,0x40,length);
                posyr += 13;
            }

            length=(int)strlen(wx_wind);
            if ( (!ghost || Select_.old_data) && length>0) {
                x_offset=((x_long-NW_corner_longitude)/scale_x)-(length*3);
                y_offset=((y_lat -NW_corner_latitude) /scale_y)+posyr;
                draw_nice_string(w,where,letter_style,x_offset,y_offset,wx_wind,0x08,0x40,length);
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
    int ghost;
    int skip_duplicates;


    // See if we should draw multipoints for this station. This only happens
    // if there are points to draw and the object has not been cleared (or 
    // we're supposed to show old data).

    // Per Dale Huguley in e-mail 07/10/2003, a good interval for
    // the severe weather polygons to disappear is 10 minutes.  We
    // hard-code it here so the user can't mess it up too badly with
    // the ghosting interval.
//    ghost = (int)(((sec_old+sec_heard)) < sec_now());
    ghost = (int)( ( sec_heard + (10 * 60) ) < sec_now() );

    // We don't want to draw them if the ghost interval is up, no
    // matter whether Include Expired Data is checked.
    //if ( (!ghost || Select_.old_data) && (numpoints > 0) ) {
    if ( !ghost  && (numpoints > 0) ) {

        //long x_offset, y_offset;
        int  i;
//        XPoint xpoints[MAX_MULTIPOINTS + 1];

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
#else   // 0

        // See if the station icon is on the screen. If so, draw the associated
        // points. The drawback to this approach is that if the station icon is
        // scrolled off the edge of the display the points aren't drawn even if
        // one or more of them is on the display.

//        if( (x_long > NW_corner_longitude) && (x_long < SE_corner_longitude)
//            && (y_lat > NW_corner_latitude) && (y_lat < SE_corner_latitude) )
#endif  // 0
        {
            //x_offset = (x_long - NW_corner_longitude) / scale_x;
            //y_offset = (y_lat - NW_corner_latitude) / scale_y;

            // Convert each of the points from Xastir coordinates to
            // screen coordinates and fill in the xpoints array.

//            for (i = 0; i < numpoints; ++i) {
//                xpoints[i].x = (mypoints[i][0] - NW_corner_longitude) / scale_x;
//                xpoints[i].y = (mypoints[i][1] - NW_corner_latitude) / scale_y;
//                // fprintf(stderr,"   %d: %d,%d\n", i, xpoints[i].x, xpoints[i].y);
//            }

            // The type parameter determines how the points will be used.
            // After determining the type, use the style parameter to
            // get the color and line style.

            switch (type) {

                case '0':           // closed polygon
                default:
                    // Repeat the first point so the polygon will be closed.

//                    xpoints[numpoints].x = xpoints[0].x;
//                    xpoints[numpoints].y = xpoints[0].y;

                    // First draw a wider black line.
                    (void)XSetForeground(XtDisplay(da), gc, colors[0x08]);  // black
                    (void)XSetLineAttributes(XtDisplay(da), gc, 4, LineSolid, CapButt, JoinMiter);

                    skip_duplicates = 0;
                    for (i = 0; i < numpoints-1; i++) {
//                        (void)XDrawLines(XtDisplay(da), where, gc, xpoints, numpoints+1, CoordModeOrigin);
                        draw_vector(da, mypoints[i][0],
                            mypoints[i][1],
                            mypoints[i+1][0],
                            mypoints[i+1][1],
                            gc,
                            where,
                            skip_duplicates);

                        skip_duplicates = 1;
                    }
                    // Close the polygon
                    draw_vector(da,
                        mypoints[i][0],
                        mypoints[i][1],
                        mypoints[0][0],
                        mypoints[0][1],
                        gc,
                        where,
                        skip_duplicates);

                    // Then draw the appropriate colored line on top of it.
                    (void)XSetForeground(XtDisplay(da), gc, getLineColor(style));
                    (void)XSetLineAttributes(XtDisplay(da), gc, 2, getLineStyle(style), CapButt, JoinMiter);

                    skip_duplicates = 0;
                    for (i = 0; i < numpoints-1; i++) {
//                        (void)XDrawLines(XtDisplay(da), where, gc, xpoints, numpoints+1, CoordModeOrigin);
                        draw_vector(da, mypoints[i][0],
                            mypoints[i][1],
                            mypoints[i+1][0],
                            mypoints[i+1][1],
                            gc,
                            where,
                            skip_duplicates);

                        skip_duplicates = 1;
                    }
                    // Close the polygon
                    draw_vector(da,
                        mypoints[i][0],
                        mypoints[i][1],
                        mypoints[0][0],
                        mypoints[0][1],
                        gc,
                        where,
                        skip_duplicates);

                    break;

                case '1':           // line segments

                    (void)XSetForeground(XtDisplay(da), gc, getLineColor(style));
                    (void)XSetLineAttributes(XtDisplay(da), gc, 2, getLineStyle(style), CapButt, JoinMiter);

                    skip_duplicates = 0;
                    for (i = 0; i < numpoints-1; i++) {
//                        (void)XDrawLines(XtDisplay(da), where, gc, xpoints, numpoints, CoordModeOrigin);
                        draw_vector(da, mypoints[i][0],
                            mypoints[i][1],
                            mypoints[i+1][0],
                            mypoints[i+1][1],
                            gc,
                            where,
                            skip_duplicates);

                        skip_duplicates = 1;
                    }

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
    int i = XTPOINTER_TO_INT(clientData);

    //fprintf(stderr,"Selected a symbol: %d\n", clientData);

    if ( i > 0) {
        //fprintf(stderr,"Symbol is from primary symbol table: /%c\n",(char)i);
        table[0] = '/';
        symbol[0] = (char)i;
    }
    else {
        //fprintf(stderr,"Symbol is from secondary symbol table: \\%c\n",(char)(-i));
        table[0] = '\\';
        symbol[0] = (char)(-i);
    }
    table[1] = '\0';
    symbol[1] = '\0';


    if (symbol_change_requested_from == 1) {        // Configure->Station Dialog
        symbol_change_requested_from = 0;
        //fprintf(stderr,"Updating Configure->Station Dialog\n");

        XmTextFieldSetString(station_config_group_data,table);
        XmTextFieldSetString(station_config_symbol_data,symbol);
        updateSymbolPictureCallback(widget,clientData,callData);
    }
    else if (symbol_change_requested_from == 2) {   // Create->Object/Item Dialog
        symbol_change_requested_from = 0;
        //fprintf(stderr,"Updating Create->Object/Item Dialog\n");

        XmTextFieldSetString(object_group_data,table);
        XmTextFieldSetString(object_symbol_data,symbol);
        updateObjectPictureCallback(widget,clientData,callData);
    }
    else {  // Do nothing.  We shouldn't be here.
        symbol_change_requested_from = 0;
    }

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
                xmDialogShellWidgetClass, appshell,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                    XmNfontList, fontlist1,
                    NULL);

            symbol(b1,0,'/',(char)i,' ',select_icons[i-33],0,0,0,' ');  // create icon

            // Here we send back the ascii number of the symbol.  We need to keep it within
            // the range of short int's.
            XtAddCallback(b1,
                XmNactivateCallback,
                Select_symbol_change_data,
                INT_TO_XTPOINTER(i) );
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
                    XmNfontList, fontlist1,
                    NULL);

            symbol(b1,0,'\\',(char)i-94,' ',select_icons[i-33],0,0,0,' ');  // create icon

            // Here we send back the ascii number of the symbol negated.  We need to keep it
            // within the range of short int's.
            XtAddCallback(b1,
                XmNactivateCallback,
                Select_symbol_change_data,
                INT_TO_XTPOINTER(-(i-94)) );
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
                XmNfontList, fontlist1,
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





// Function to draw dead-reckoning symbols.
//
void draw_deadreckoning_features(DataRow *p_station,
                                 Pixmap where,
                                 Widget w) {
    double my_course;
    long x_long, y_lat;
    long x_long2, y_lat2;
    long x, y;
    long x2, y2;
    double diameter;
    int color = trail_colors[p_station->trail_color];
//    int symbol_on_screen = 0;
    int ghosted_symbol_on_screen = 0;


// This function takes a bit of CPU if we are zoomed out.  It'd be
// best to check first whether the zoom level and the speed make it
// worth computing DR at all for this station.  As a first
// approximation, we could turn off DR if we're at zoom 8000 or
// higher:
//
//    if (scale_y > 8000)
//        return;


    x_long = p_station->coord_lon;
    y_lat = p_station->coord_lat;

    // x/y are screen locations for start position
    x = (x_long - NW_corner_longitude)/scale_x;
    y = (y_lat - NW_corner_latitude)/scale_y;

    y_lat2 = y_lat;
    x_long2 = x_long;

    // Compute the latest DR'ed position for the object
    compute_current_DR_position(p_station,
        &x_long2,
        &y_lat2);

    // x2/y2 are screen location for ghost symbol (DR'ed position)
    x2 = (x_long2 - NW_corner_longitude)/scale_x;
    y2 = (y_lat2 - NW_corner_latitude)/scale_y;


    // Check DR'ed symbol position
    if (    (x_long2>NW_corner_longitude) &&
            (x_long2<SE_corner_longitude) &&
            (y_lat2>NW_corner_latitude) &&
            (y_lat2<SE_corner_latitude) &&
            ((x_long>=0) && (x_long<=129600000l)) &&
            ((y_lat>=0) && (y_lat<=64800000l))) {

        ghosted_symbol_on_screen++;
    }


    // Draw the DR arc
    //
    if (Display_.dr_arc && ghosted_symbol_on_screen) {

        double xdiff, ydiff;


        xdiff = (x2-x) * 1.0;
        ydiff = (y2-y) * 1.0;

        // a squared + b squared = c squared
        diameter = 2.0 * sqrt( (double)( (ydiff*ydiff) + (xdiff*xdiff) ) );

        //fprintf(stderr,"Range:%f\tDiameter:%f\n",range,diameter);

        if (diameter > 10.0) {
            int arc_degrees = (sec_now() - p_station->sec_heard) * 90 / (5*60);

            if (arc_degrees > 360) {
                arc_degrees = 360;
        }

        (void)XSetLineAttributes(XtDisplay(da), gc, 1, LineOnOffDash, CapButt,JoinMiter);
        //(void)XSetForeground(XtDisplay(da),gc,colors[0x0a]);
        //(void)XSetForeground(XtDisplay(da),gc,colors[0x44]); // red3
        (void)XSetForeground(XtDisplay(da),gc,color);


        // Compute angle from the two screen positions.
        //
        if (xdiff != 0) {
//We should guard against divide-by-zero here!
             my_course = 57.29578 * atan(xdiff/ ydiff);
        }
        else {
            if (ydiff >= 0) {
                my_course = 180.0;
            }
            else {
                my_course = 0.0;
            }
        }


//fprintf(stderr,"my_course:%f\n", my_course);
        // The arctan function returns values between -90 and +90.  To
        // obtain the true course we apply the following rules:
        if (ydiff > 0 && xdiff > 0) {
//fprintf(stderr,"1\n");  // Lower-right quadrant
            my_course = 360.0 - my_course;
        }
        else if (ydiff < 0.0 && xdiff > 0.0) {
//fprintf(stderr,"2\n");  // Upper-right quadrant
            my_course = 180.0 - my_course;
        }
        else if (ydiff < 0.0 && xdiff < 0.0) {
//fprintf(stderr,"3\n");  // Upper-left quadrant
            my_course = 180.0 - my_course;
        }
        else if (ydiff > 0.0 && xdiff < 0.0) {
//fprintf(stderr,"4\n");  // Lower-left quadrant
            my_course = 360.0 - my_course;
        }
        else {  // 0/90/180/270
//fprintf(stderr,"5\n");
            my_course = 180.0 + my_course;
        }


        // Convert to screen angle for XDrawArc routines:
        my_course = my_course + 90.0;

        if (my_course > 360.0)
            my_course = my_course - 360.0;
 
//fprintf(stderr,"\tmy_course2:%f\n", my_course);

// Check that our parameters are within spec for XDrawArc.  Tricky
// 'cuz the XArc struct has short's and unsigned short's, while
// XDrawArc man-page says int's/unsigned int's.  We'll stick to 16-bit
// just to make sure.

        (void)XDrawArc(XtDisplay(da),where,gc,
            l16(x-(diameter/2)),    // int
            l16(y-(diameter/2)),    // int
            lu16(diameter),         // unsigned int
            lu16(diameter),         // unsigned int
            l16(-64*my_course),     // int
            l16(64/2*arc_degrees)); // int

// Check that our parameters are within spec for XDrawArc.  Tricky
// 'cuz the XArc struct has short's and unsigned short's, while
// XDrawArc man-page says int's/unsigned int's.  We'll stick to 16-bit
// just to make sure.

        (void)XDrawArc(XtDisplay(da),where,gc,
            l16(x-(diameter/2)),        // int
            l16(y-(diameter/2)),        // int
            lu16(diameter),             // unsigned int
            lu16(diameter),             // unsigned int
            l16(-64*my_course),         // int
            l16(-64/2*arc_degrees));    // int
        }
    }


// Note that for the DR course, if we're in the middle of the symbol
// and the DR'ed symbol (ghosted symbol), but neither of them are
// on-screen, the DR'ed course won't display.
//
    // Draw the DR'ed course if either the symbol or the DR'ed
    // symbol (ghosted symbol) are on-screen.
    //
    if (Display_.dr_course) {

        (void)XSetLineAttributes(XtDisplay(da), gc, 0, LineOnOffDash, CapButt,JoinMiter);
        (void)XSetForeground(XtDisplay(da),gc,color); // red3

        // This one changes the angle as the vector gets longer, by
        // about 10 degrees (A test at 133 degrees -> 143 degrees).
        // draw_vector() needs to truncate the line in this case,
        // maintaining the same slope.  This behavior is _much_
        // better than the XDrawLine above though!
        //
        draw_vector(w,
            x_long,
            y_lat,
            x_long2,
            y_lat2,
            gc,
            where,
            0);
    }


    // Draw the DR'ed symbol (ghosted symbol) if enabled and if
    // on-screen.
    //
    if (Display_.dr_symbol && ghosted_symbol_on_screen) {

        draw_symbol(w,
            p_station->aprs_symbol.aprs_type,
            p_station->aprs_symbol.aprs_symbol,
            p_station->aprs_symbol.special_overlay,
            x_long2,
            y_lat2,
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            p_station->sec_heard-sec_old,   // Always draw it ghosted
            0,
            where,
            symbol_orient(p_station->course),
            p_station->aprs_symbol.area_object.type,
            p_station->signpost,
            0); // Don't bump the station count
    }
}


