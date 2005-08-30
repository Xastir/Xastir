/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2005  The Xastir Group
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

extern int npoints;		/* tsk tsk tsk -- globals */
extern int mag;


// Note:  There are long's in the pdb_hdr that are not being used.
// If they were, the ntohl() function would need to be used in order
// to make sure they were represented correctly on big-endian and
// little-endian machines.  Same goes for short's, make sure ntohs()
// is used in all cases.
//
// Also Note: It looks like all the values in the data structures are 
// unsigned, we may have to explicitly define the long's as well.  -KD6ZWR
//
void draw_palm_image_map(Widget w, 
			 char *dir,
			 char *filenm,
			 alert_entry *alert, 
			 u_char alert_color,
			 int destination_pixmap, 
			 map_draw_flags *mdf) {

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
    char short_filenm[MAX_FILENAME];
    int records, record_count, count;
    int scale;
    long map_left, map_right, map_top, map_bottom, max_x, max_y;
    long record_ptr;
    long line_x, line_y;
    int vector;
    char status_text[MAX_FILENAME];
    int draw_filled;

    draw_filled = mdf->draw_filled;

    xastir_snprintf(filename, sizeof(filename), "%s/%s", dir, filenm);

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
                map_right,  // Right
                1000);      // Default Map Level

            fclose(fn);

            // Update the statusline for this map name.
            xastir_snprintf(status_text,
                sizeof(status_text),
                langcode ("BBARSTA039"),
                short_filenm);
            statusline(status_text,0);       // Loading/Indexing ...

            return; // Done indexing this file
        }


        if (map_onscreen(map_left, map_right, map_top, map_bottom, 1)) {


            // Update the statusline for this map name
            xastir_snprintf(status_text,
                sizeof(status_text),
                langcode ("BBARSTA028"),
                short_filenm);
            statusline(status_text,0);       // Loading/Indexing ...


            max_x = (long)(screen_width + MAX_OUTBOUND);
            max_y = (long)(screen_height + MAX_OUTBOUND);


            HandlePendingEvents(app_context);
            if (interrupt_drawing_now) {
                fclose(fn);
                // Update to screen
                (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
                return;
            }


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
                                                label_record.text,
                                                FONT_DEFAULT);
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


