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
void draw_gnis_map (Widget w,
		    char *dir,
		    char *filenm,
		    alert_entry *alert,
		    u_char alert_color,
		    int destination_pixmap,
		    int draw_filled)
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


    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        return;
    }


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




