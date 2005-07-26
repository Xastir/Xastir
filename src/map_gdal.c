/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2004-2005  The Xastir Group
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
 */


#define TIGER_POLYGONS


//
// NOTE:  GDAL sample data can be found here:
//
//          ftp://ftp.remotesensing.org/gdal/data
// or here: http://gdal.maptools.org/dl/data/
//


#include "config.h"
#include "snprintf.h"

//#include <stdio.h>
#include <stdlib.h>
//#include <unistd.h>
//#include <sys/stat.h>
//#include <ctype.h>
//#include <sys/types.h>
//#include <pwd.h>
//#include <errno.h>

// Needed for Solaris
//#include <strings.h>

//#include <dirent.h>
//#include <netinet/in.h>
//#include <Xm/XmAll.h>

//#ifdef HAVE_X11_XPM_H
//#include <X11/xpm.h>
//#ifdef HAVE_LIBXPM // if we have both, prefer the extra library
//#undef HAVE_XM_XPMI_H
//#endif // HAVE_LIBXPM
//#endif // HAVE_X11_XPM_H

//#ifdef HAVE_XM_XPMI_H
//#include <Xm/XpmI.h>
//#endif // HAVE_XM_XPMI_H

//#include <X11/Xlib.h>

//#include <math.h>

#include "maps.h"
#include "alert.h"
//#include "util.h"
#include "main.h"
//#include "datum.h"
#include "draw_symbols.h"
//#include "rotated.h"
//#include "color.h"
//#include "xa_config.h"

#ifdef TIGER_POLYGONS
#include "hashtable.h"
#include "hashtable_itr.h"
#endif  // TIGER_POLYGONS


#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }



#ifdef HAVE_LIBGDAL

//#warning
//#warning
//#warning GDAL/OGR library support is not fully implemented yet!
//#warning Preliminary TIGER/Line, Shapefile, mid/mif/tab (MapInfo),
//#warning and SDTS support is functional, including on-the-fly
//#warning coordinate conversion for both indexing and drawing.
//#warning
//#warning

// Getting rid of stupid compiler warnings in GDAL
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

#include "gdal.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"
#include "cpl_string.h"

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
#endif  // HAVE_LIBGDAL


// Needs to be down here so that the LIB_GC stuff will compile in
// ok.
#include "xastir.h"



void map_gdal_init() {

#ifdef HAVE_LIBGDAL

    GDALDriverH  gDriver;   // GDAL driver
    OGRSFDriverH oDriver;   // OGR driver
    int ii;
    int jj;


    // Register all known GDAL drivers
    GDALAllRegister();

    // Register all known OGR drivers
    OGRRegisterAll();

    // Print out each supported GDAL driver.
    //
    ii = GDALGetDriverCount();

//    fprintf(stderr,"  GDAL Registered Drivers:\n");
    for (jj = 0; jj < ii; jj++) {
        gDriver = GDALGetDriver(jj);
//        fprintf(stderr,"%10s   %s\n",
//            GDALGetDriverShortName(gDriver),
//            GDALGetDriverLongName(gDriver) );
    }
//    fprintf(stderr,"             (none)\n");


    // Print out each supported OGR driver
    //
    ii = OGRGetDriverCount();

//    fprintf(stderr,"  OGR Registered Drivers:\n");

    for  (jj = 0; jj < ii; jj++) {
        char *drv_name;


        oDriver = OGRGetDriver(jj);
        drv_name = (char *)OGR_Dr_GetName(oDriver);

        if (strstr(drv_name,"Shapefile")) {
#ifdef GDAL_SHAPEFILES
            fprintf(stderr,"       shp   ESRI Shapefile (via GDAL)\n");
#endif  // GDAL_SHAPEFILES
        }
        else if (strstr(drv_name,"NTF")) {
            // Not enabled in Xastir yet
        }
        else if (strstr(drv_name,"SDTS")) {
            fprintf(stderr,"       ddf   Spatial Data Transfer Standard (SDTS)\n");
        }
        else if (strstr(drv_name,"TIGER")) {
            fprintf(stderr,"       rt1   US Census Bureau TIGER/Line\n");
        }
        else if (strstr(drv_name,"S57")) {
            fprintf(stderr,"       s57   International Hydrographic Organization (IHO) S-57\n");
        }
        else if (strstr(drv_name,"MapInfo")) {
            fprintf(stderr,"       tab   MapInfo TAB\n");
            fprintf(stderr,"       mid   MapInfo MID\n");
            fprintf(stderr,"       mif   MapInfo MIF\n");
        }
        else if (strstr(drv_name,"DGN")) {
            fprintf(stderr,"       dgn   MicroStation DGN\n");
        }
        else if (strstr(drv_name,"VRT")) {
            // Not enabled in Xastir yet
        }
        else if (strstr(drv_name,"AVCBin")) {
            // Not enabled in Xastir yet
        }
        else if (strstr(drv_name,"REC")) {
            // Not enabled in Xastir yet
        }
        else if (strstr(drv_name,"Memory")) {
            // Not enabled in Xastir yet
        }
        else if (strstr(drv_name,"GML")) {
            // Not enabled in Xastir yet
        }
        else if (strstr(drv_name,"PostgreSQL")) {
            // Not enabled in Xastir yet
        }
    }
    fprintf(stderr,"\n");

#endif  // HAVE_LIBGDAL

}





/*  TODO: 
 *    projections
 *    multiple bands 
 *    colormaps 
 *    .geo files vs. .wld
 *    map index
 */


#ifdef HAVE_LIBGDAL

void draw_gdal_map(Widget w,
                   char *dir,
                   char *filenm,
                   alert_entry *alert,
                   u_char alert_color,
                   int destination_pixmap,
                   map_draw_flags *mdf) {

    GDALDatasetH hDataset;
    char file[MAX_FILENAME];    // Complete path/name of image
    GDALDriverH   hDriver;
    double adfGeoTransform[6];
//    double map_x_mind, map_x_maxd, map_dxd; // decimal degrees
//    double map_y_mind, map_y_maxd, map_dyd; // decimal degrees
    unsigned long map_x_min, map_x_max; // xastir units
    unsigned long map_y_min, map_y_max; // xastir units
    long map_dx, map_dy; // xastir units
    double scr_m_dx, scr_m_dy; // screen pixels per map pixel
    unsigned long map_s_x_min, map_s_x_max, map_s_x; // map pixels
//    unsigned long map_s_y_min, map_s_y_max, map_s_y; // map pixels
    unsigned long scr_s_x_min, scr_s_x_max, scr_s_x; // screen pixels
//    unsigned long scr_s_y_min, scr_s_y_max, scr_s_y; // screen pixels
        
    // 
    GDALRasterBandH hBand = NULL;
    int numBand, hBandc;
    int nBlockXSize, nBlockYSize;
    //    int bGotMin, bGotMax;
    //    double adfMinMax[2];
    //
    const char *pszProjection;
    float *pafScanline;
    int nXSize;
    unsigned int width,height;
    GDALColorTableH hColorTable;
    GDALColorInterp hColorInterp;
    GDALPaletteInterp hPalInterp;
    GDALColorEntry colEntry;
    GDALDataType hType;
    double dfNoData;
    int bGotNodata;
    int i;

    xastir_snprintf(file, sizeof(file), "%s/%s", dir, filenm);

    if ( debug_level & 16 ) 
        fprintf(stderr,"Calling GDALOpen on file: %s\n", file);

    hDataset = GDALOpenShared( file, GA_ReadOnly );

    if( hDataset == NULL ) {
        fprintf(stderr,"GDAL couldn't open file: %s\n", file);
    }


    if ( debug_level & 16 ) {
        hDriver = GDALGetDatasetDriver( hDataset );
        fprintf( stderr, "Driver: %s/%s\n",
                 GDALGetDriverShortName( hDriver ),
                 GDALGetDriverLongName( hDriver ) );
        
        fprintf( stderr,"Size is %dx%dx%d\n",
                 GDALGetRasterXSize( hDataset ), 
                 GDALGetRasterYSize( hDataset ),
                 GDALGetRasterCount( hDataset ) );
    
        if( GDALGetProjectionRef( hDataset ) != NULL )
            fprintf( stderr,"Projection is `%s'\n", GDALGetProjectionRef( hDataset ) );
    
        if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None ) {
                fprintf( stderr,"Origin = (%.6f,%.6f)\n",
                         adfGeoTransform[0], adfGeoTransform[3] );
            
                fprintf( stderr,"Pixel Size = (%.6f,%.6f)\n",
                         adfGeoTransform[1], adfGeoTransform[5] );
            }
    }
    pszProjection = GDALGetProjectionRef( hDataset );
    GDALGetGeoTransform( hDataset, adfGeoTransform );    
    numBand = GDALGetRasterCount( hDataset );
    width = GDALGetRasterXSize( hDataset );
    height = GDALGetRasterYSize( hDataset );
    // will have to delay these calcs until after a proj call
    map_x_min = 64800000l + (360000.0 * adfGeoTransform[0]);
    map_x_max = 64800000l + (360000.0 * (adfGeoTransform[0] + adfGeoTransform[1] * width));
    map_y_min = 32400000l + (360000.0 * (-adfGeoTransform[3] ));
    map_y_max = 32400000l + (360000.0 * (-adfGeoTransform[3] + adfGeoTransform[5] * height));
    map_dx = adfGeoTransform[1] * 360000.0;
    map_dy = adfGeoTransform[5] * 360000.0;
    scr_m_dx = scale_x / map_dx;
    scr_m_dy = scale_y / map_dy;

    /*
     * Here are the corners of our viewport, using the Xastir
     * coordinate system.  Notice that Y is upside down:
     *
     *   left edge of view = x_long_offset
     *  right edge of view = x_long_offset + (screen_width  * scale_x)
     *    top edge of view =  y_lat_offset
     * bottom edge of view =  y_lat_offset + (screen_height * scale_y)
     *
     * The corners of our map:
     *
     *   left edge of map = map_x_min   in Xastir format
     *  right edge of map = map_x_max
     *    top edge of map = map_y_min
     * bottom edge of map = map_y_max
     * 
     * Map scale in xastir units per pixel: map_dx and map_dy
     * Map scale in screen pixels per map pixel: scr_m_dx and scr_m_dy
     *
     * map pixel offsets that are on screen:
     *   left edge of map = map_s_x_min
     *  right edge of map = map_s_x_max
     *    top edge of map = map_s_y_min
     * bottom edge of map = map_s_y_max
     * 
     * screen pixel offsets for map edges:
     *   left edge of map = scr_s_x_min
     *  right edge of map = scr_s_x_max
     *    top edge of map = scr_s_y_min
     * bottom edge of map = scr_s_y_max
     * 
     */


// Setting some variables that haven't been set yet.
map_s_x_min = 0;
scr_s_x_min = 0;


    // calculate map range on screen
    scr_s_x_max = map_s_x_max = 0ul;
    for ( map_s_x = 0, scr_s_x = 0; map_s_x_min < width; map_s_x_min++, scr_s_x_min += scr_m_dx) {
        if ((x_long_offset + (scr_s_x_min * scale_x)) > (map_x_min + (map_s_x_min * map_dx))) 
            map_s_x_min = map_s_x;
        
        
    }

    for (hBandc = 0; hBandc < numBand; hBandc++) {
        hBand = GDALGetRasterBand( hDataset, hBandc + 1 );
        dfNoData = GDALGetRasterNoDataValue( hBand, &bGotNodata );
        GDALGetBlockSize( hBand, &nBlockXSize, &nBlockYSize );
        hType = GDALGetRasterDataType(hBand);
        
        if ( debug_level & 16 ) {
            fprintf(stderr, "Band %d (/%d):\n", hBandc, numBand); 
            fprintf( stderr, " Block=%dx%d Type=%s.\n",
                     nBlockXSize, nBlockYSize,
                     GDALGetDataTypeName(hType) );
            if( GDALGetDescription( hBand ) != NULL 
                && strlen(GDALGetDescription( hBand )) > 0 )
                fprintf(stderr, " Description = %s\n", GDALGetDescription(hBand) );
            if( strlen(GDALGetRasterUnitType(hBand)) > 0 )
                fprintf(stderr, " Unit Type: %s\n", GDALGetRasterUnitType(hBand) );
            if( bGotNodata )
                fprintf(stderr, " NoData Value=%g\n", dfNoData );
        }
        // handle colormap stuff

        hColorInterp = GDALGetRasterColorInterpretation ( hBand );
        if ( debug_level & 16 ) 
            fprintf ( stderr, " Band's Color is interpreted as %s.\n", 
                      GDALGetColorInterpretationName (hColorInterp));

        if( GDALGetRasterColorInterpretation(hBand) == GCI_PaletteIndex ) {

            hColorTable = GDALGetRasterColorTable( hBand );
            hPalInterp = GDALGetPaletteInterpretation ( hColorTable );

            if ( debug_level & 16 ) 
                fprintf( stderr, " Band has a color table (%s) with %d entries.\n", 
                         GDALGetPaletteInterpretationName( hPalInterp),
                         GDALGetColorEntryCount( hColorTable ) );

            for( i = 0; i < GDALGetColorEntryCount( hColorTable ); i++ ) {
                    GDALGetColorEntryAsRGB( hColorTable, i, &colEntry );
                    fprintf( stderr, "  %3d: %d,%d,%d,%d\n", 
                             i, 
                             colEntry.c1,
                             colEntry.c2,
                             colEntry.c3,
                             colEntry.c4 );
                }
        } // PaletteIndex
        

    }
    //

        if ( numBand == 1) { // single band

        }
        if ( numBand == 3) { // seperate RGB bands
        
        }




    
    nXSize = GDALGetRasterBandXSize( hBand );

    pafScanline = (float *) malloc(sizeof(float)*nXSize);
    CHECKMALLOC(pafScanline);

    GDALRasterIO( hBand, GF_Read, 0, 0, nXSize, 1, 
                  pafScanline, nXSize, 1, GDT_Float32, 
                  0, 0 );

    /* The raster is */






    GDALClose(hDataset);
}



/////////////////////////////////////////////////////////////////////
// Above this point is GDAL code (raster), below is OGR code
// (vector).
/////////////////////////////////////////////////////////////////////



//WE7U
//
// Possible Optimizations:
// -----------------------
//
// For Tiger and/or SDTS:  If we can figure out what type of map we
// have, only load those layers that make sense.  Skip the rest.
//
// Only change the drawing style/color if it is different than the
// last specified one.  Have the drawing routines actually make the
// X11 calls instead of the "guess" function, in order to eliminate
// extra calls.
//
// Compute the spatial transforms for a layer only if it has a
// different spatial reference from the last layer.
//
// Break the "guess" function into one for files, one for layers,
// and perhaps one for features.  Call the "features" one sparingly
// if it exists, so that we can skip that code most of the time.
//
// Re-write the "guess" function so that it turns into a general map
// preferences feature.  Make sure to use hash tables to keep it
// fast.
//
// Tie the Tigermap config buttons into the TIGER/Line layers as
// well, skip those layers that we don't have selected.  Perhaps tie
// Tiger/Shapefiles into the same scheme as those have specific
// filenames we can key off of.



int label_color_guess;
int sdts_elevation_in_meters = 1;   // Default meters
int hypsography_layer = 0;          // Topo contours
int hydrography_layer = 0;          // Underwater contours
int water_layer = 0;
int roads_trails_layer = 0;
int railroad_layer = 0;
int misc_transportation_layer = 0;



// guess_vector_attributes()
//
// Feel free to change the name.  At the moment it's somewhat
// appropriate, but I would hope that in the future it won't be.
// This should tie in nicely to the dbfawk stuff, and perhaps other
// schemes for the user to determine drawing attributes.  Basically,
// if dbfawk is compiled in, go do that instead of doing what this
// routine does.
//
// What we need to do:  Come up with a signature match for the
// driver type, filename, layer, and sometimes object_type and
// file_attributes.  Use that to determine drawing width, label
// size/placement, color, etc.  If the signature could also specify
// how often the signature needs to be re-examined, that would be a
// plus.  If it only needs to be set once per file, we could save a
// lot of time.
//
// Note that draw_polygon_with_mask() uses gc_temp instead of gc.
// This means we have to pass colors down to it so that it can set
// the color itself.  We currently just use the label_color variable
// to do this, but we really should use a different variable, as
// label color and object color can be different.
//
// Depending on what needs to be done for different file
// types/layers/geometries, we might want to break this function up
// into several, so that we can optimize for speed.  If something
// only needs to be set once per file, or once per layer, do so.
// Don't set it over and over again (Don't set it once per object
// drawn).
//
// Consider trying GXxor function with gc_tint someday to see what
// the contour lines look like with that method:  Should make them
// quite readable no matter what the underlying map colors.
//
// Set attributes based on what sort of file/layer/shape we're
// dealing with.  driver_type may be any of:
//
// "AVCbin"
// "DGN"
// "FMEObjects Gateway"
// "GML"
// "Memory"
// "MapInfo File"
// "UK .NTF"
// "OCI"
// "ODBC"
// "OGDI"
// "PostgreSQL"
// "REC"
// "S57"
// "SDTS"
// "ESRI Shapefile"
// "TIGER"
// "VRT"
//
// Shapefiles:  If Mapshots or ESRI tiger->Shapefiles, guess the
// coloring and line widths based on the filename and the associated
// field contents.
//
// SDTS:  Guess the coloring/line widths based on the ENTITY_LABEL.
//
// TIGER:  Guess coloring/line widths based on the CFCC field.
//
// MapInfo:  Guess based on layer?
//
// DGN: ??
//
//
void guess_vector_attributes( Widget w,
                              const char *driver_type,
                              char *full_filename,
                              OGRLayerH layerH,
                              OGRFeatureH featureH,
                              int geometry_type ) {
    int ii;
    const char *pii = NULL;

    label_color_guess = -1; // Default = don't draw the feature


//fprintf(stderr,"guess: %s\n", full_filename);


    // Default line type
    (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineSolid, CapButt,JoinMiter);

 
/*
    switch (driver_type) {
        default:
            break;
    }

 
    switch (layerH) {
        case 0:
        case 1:
        default:
            (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]);  // black
            break;
    }
*/



    // SDTS FILES
    // ----------
    // Check the ENTITY_LABEL.  That gives us very detailed info
    // about the type of feature we're dealing with so that we can
    // decide how to draw it.
    //
    if (strstr(driver_type,"SDTS")) {
 
        ii = OGR_F_GetFieldIndex( featureH, "ENTITY_LABEL");
        if (ii != -1) {
            const char *entity_num;

            entity_num = OGR_F_GetFieldAsString( featureH, ii);

//fprintf(stderr,"ENTITY_LABEL: %s\n", entity_num);

            if (strncmp(entity_num,"020",3) == 0) {
                // Contour lines
                (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x0e]);  // yellow
                label_color_guess = 0x0e;   // yellow
            }
            else if (strncmp(entity_num,"050",3) == 0) {
                // hydrography
                (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x0e]);  // yellow
                label_color_guess = 0x0e;   // yellow
            }
            else if (strncmp(entity_num,"070",3) == 0) {
                // vegetative surface cover
                label_color_guess = -1;
                return; // Don't display it
            }
            else if (strncmp(entity_num,"080",3) == 0) {
                // non-vegetative features
                label_color_guess = -1;
                return; // Don't display it
            }
            else if (strncmp(entity_num,"090",3) == 0) {
                // boundaries
                label_color_guess = -1;
                return; // Don't display it
            }
            else if (strncmp(entity_num,"150",3) == 0) {
                // survey control and markers
                label_color_guess = -1;
                return; // Don't display it
            }
            else if (strncmp(entity_num,"170",3) == 0) {
                // roads and trails
                (void)XSetForeground(XtDisplay(da), gc, colors[(int)0x08]);  // black
                label_color_guess = 0x08;
            }
            else if (strncmp(entity_num,"180",3) == 0) {
                // railroads
                (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineOnOffDash, CapButt,JoinMiter);
                (void)XSetForeground(XtDisplay(da), gc, colors[(int)0x01]);  // purple
                label_color_guess = 0x01;
            }
            else if (strncmp(entity_num,"190",3) == 0) {
                // pipelines, transmissions lines, misc transportation
                (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineOnOffDash, CapButt,JoinMiter);
                (void)XSetForeground(XtDisplay(da), gc, colors[(int)0x4d]);  // white
                label_color_guess = 0x4d;
            }
            else if (strncmp(entity_num,"200",3) == 0) {
                // man-made features
                label_color_guess = -1;
                return; // Don't display it
            }
            else if (strncmp(entity_num,"300",3) == 0) {
                // public land survey system
                label_color_guess = -1;
                return; // Don't display it
            }
        }
        return;
    }   // End of SDTS
 


    // RAW TIGER FILES
    // ---------------
    // Attempt to snag the CFCC field.  This tells us what the
    // feature is and we can get clues from it as to how to draw the
    // feature.
    //
    if (strstr(driver_type,"TIGER")) {
 
        ii = OGR_F_GetFieldIndex(featureH, "CFCC");

        if (ii != -1) {  // Found one of the fields

            pii = OGR_F_GetFieldAsString(featureH, ii);

//fprintf(stderr,"CFCC: %s\n", pii);

            if (!pii)
                return;

            switch (pii[0]) {

                case 'A':   // Road
                case 'P':   // Provisional Road
                    (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]);  // black
                    label_color_guess = 0x08;   // black
                    break;

                case 'B':   // Railroad
                    (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineOnOffDash, CapButt,JoinMiter);
                    (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x01]);  // purple
                    label_color_guess = 0x01;   // purple
                    break;

                case 'C':   // Misc Ground Transportation
                    (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineOnOffDash, CapButt,JoinMiter);
                    (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x4d]);  // white
                    label_color_guess = 0x4d;   // white
                    break;

                case 'D':   // Landmark
                    (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]);  // black
                    label_color_guess = 0x08;   // black
                    break;

                case 'E':   // Physical Feature
                    (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]);  // black
                    label_color_guess = 0x08;   // black
                    break;

                case 'F':   // Non-visible Feature
                    label_color_guess = -1;
                    return; // Don't display it!
                    break;
    
                case 'H':   // Hydrography (water)
                    water_layer++;
                    (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x1a]);  // Steel Blue
                    label_color_guess = 0x1a;   // Steel Blue
                    break;

                case 'X':   // Feature not yet classified
                    (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]);  // black
                    label_color_guess = 0x08;   // black
                    break;

                default:
                    (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]);  // black
                    label_color_guess = 0x08;   // black
                    break;

            }   // End of switch
        } // End of if

        return;

    }   // End of TIGER
 


    // SHAPEFILES
    // ----------
    //
    ii = OGR_F_GetFieldIndex(featureH, "LANDNAME");
    if (ii != -1) {
        (void)XSetForeground(XtDisplay(da), gc, colors[(int)0x1a]);  // Steel Blue
        label_color_guess = 0x1a;
    }



    switch (geometry_type) {


        case 1:             // Point
        case 4:             // MultiPoint
        case 0x80000001:    // Point25D
        case 0x80000004:    // MultiPoint25D

            (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]);  // black
            label_color_guess = 0x08;   // black
 
            break;


        case 2:             // LineString (polyline)
        case 5:             // MultiLineString
        case 0x80000002:    // LineString25D
        case 0x80000005:    // MultiLineString25D

            if (hypsography_layer || hydrography_layer) {

                // Set color for SDTS hypsography layer (contours)
//                (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x43]);  // gray80
                (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x0e]);  // yellow
//                label_color_guess = 0x4d;   // white
                label_color_guess = 0x0e;   // yellow
            }
            else if (roads_trails_layer) {
                (void)XSetForeground(XtDisplay(da), gc, colors[(int)0x08]);  // black
                label_color_guess = 0x08;
            }
            else if (railroad_layer) {
                (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineOnOffDash, CapButt,JoinMiter);
                (void)XSetForeground(XtDisplay(da), gc, colors[(int)0x01]);  // purple
                label_color_guess = 0x01;
            }
            else if (misc_transportation_layer) {
                (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineOnOffDash, CapButt,JoinMiter);
                (void)XSetForeground(XtDisplay(da), gc, colors[(int)0x4d]);  // white
                label_color_guess = 0x4d;
            }
            else if (strstr(full_filename,"lkH")) {
                (void)XSetForeground(XtDisplay(da), gc, colors[(int)0x1a]);  // Steel Blue
                label_color_guess = 0x1a;
            }
            else if (strstr(full_filename,"lkB")) {
                // Railroad
                (void)XSetForeground(XtDisplay(da), gc, colors[(int)0x01]);  // purple
                label_color_guess = 0x01;
            }
            else {
                (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]);  // black
                label_color_guess = 0x08;   // black
            }
            break;


        case 3:             // Polygon
        case 6:             // MultiPolygon
        case 0x80000003:    // Polygon25D
        case 0x80000006:    // MultiPolygon25D

            if (hypsography_layer || hydrography_layer) {

                // Set color for SDTS hypsography layer (contours)
//                (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x43]);  // gray80
                (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x0e]);  // yellow
//                label_color_guess = 0x4d;   // white
                label_color_guess = 0x0e;   // yellow
            }
            else if (roads_trails_layer) {
                (void)XSetForeground(XtDisplay(da), gc, colors[(int)0x08]);  // black
                label_color_guess = 0x08;
            }
            else if (railroad_layer) {
                (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineOnOffDash, CapButt,JoinMiter);
                (void)XSetForeground(XtDisplay(da), gc, colors[(int)0x01]);  // purple
                label_color_guess = 0x01;
            }
            else if (misc_transportation_layer) {
                (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineOnOffDash, CapButt,JoinMiter);
                (void)XSetForeground(XtDisplay(da), gc, colors[(int)0x4d]);  // white
                label_color_guess = 0x4d;
            }
            else if (strstr(full_filename,"wat")) {
                (void)XSetForeground(XtDisplay(da), gc, colors[(int)0x1a]);  // Steel Blue
                label_color_guess = 0x1a;
            }
            else {
                (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]);  // black
                label_color_guess = 0x08;   // black
            }
            break;


        case 7:             // GeometryCollection
        case 0x80000007:    // GeometryCollection25D
        default:            // Unknown/Unimplemented

            (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x0e]);  // yellow
            label_color_guess = 0x08;   // black
            break;
    }


//fprintf(stderr,"label_color_guess:%02x\n",label_color_guess);

}





void Draw_OGR_Labels( Widget w,
                      Pixmap pixmap,
                      OGRFeatureH featureH,
                      OGRGeometryH geometryH,
                      XPoint *xpoints,
                      int num_points,
                      int color) {

    int ii;
    const char *pii = NULL;
    char label[200] = "";
    float angle = 0.0;  // Angle for the beginning of this polyline
    int scale_factor = 10;


//fprintf(stderr,"Draw_OGR_Labels start\n");

    if (featureH == NULL)
        return; // Exit early


    // Recursively call this routine if we have a lot of points, so
    // that we draw labels at multiple points along the line.  The
    // number of points skipped should probably be tied to the zoom
    // level so that we get an appropriate number of labels at each
    // scale.  The goal should probably be two or three labels max
    // per object.
    //
    if (num_points > ((scale_factor * scale_y)+1)) {
        int skip = scale_factor * scale_y;

        Draw_OGR_Labels(w,
            pixmap,
            featureH,
            geometryH,
            &xpoints[skip],
            num_points - skip,
            color);
    }


    if (num_points > 1) {
        // Compute the label rotation angle
        float diff_X = (int)xpoints[1].x - xpoints[0].x;
        float diff_Y = (int)xpoints[1].y - xpoints[0].y;

        if (diff_X == 0.0) {  // Avoid divide by zero errors
            diff_X = 0.0000001;
        }
        if (diff_Y == 0.0) {  // Avoid divide by zero errors
            diff_Y = 0.0000001;
        }
 
        angle = atan( diff_X / diff_Y ); // Compute in radians

        // Convert to degrees
        angle = angle / (2.0 * M_PI );
        angle = angle * 360.0;

        // Change to fit our rotate label function's idea of angle
        angle = 360.0 - angle;

        if (angle >= 360.0 || angle <= 0.0)
            angle = 0.0;
    }


    //fprintf(stderr,"Y: %f\tX:
    //%f\tAngle: %f ==>
    //",diff_Y,diff_X,angle);

    if ( angle > 90.0 ) {angle += 180.0;}
    if ( angle >= 360.0 ) {angle -= 360.0;}

    //fprintf(stderr,"%f\t%s\n",angle,temp);


    // Debug code
    //OGR_F_DumpReadable( featureH, stderr );
    //OGR_G_DumpReadable(geometryH, stderr, "Shape: ");

    // Snag a few special features, use for labels
    //
    ii = OGR_F_GetFieldIndex(featureH, "NAME");
    if (ii == -1) {
        ii = OGR_F_GetFieldIndex(featureH, "FENAME");
    }
    if (ii == -1) {

        ii = OGR_F_GetFieldIndex(featureH, "ELEVATION");

        if (ii != -1) {  // Found an elevation field

            // Convert to the user's desired units based on the
            // setting of the "english_units" global variable, which
            // is set by the Enable English Units toggle.
            //
            if (english_units) {    // We desire English Units (feet)
                if (sdts_elevation_in_meters) {
                    // Map file is in meters, convert to feet for
                    // display
                    xastir_snprintf(label,
                        sizeof(label),
                        "%ift",
                        (int)((OGR_F_GetFieldAsInteger(featureH, ii) * 3.28084) + 0.5));
                }
                else {
                    // Map file is already in feet
                    xastir_snprintf(label,
                        sizeof(label),
                        "%sft",
                        OGR_F_GetFieldAsString(featureH, ii));
                }
            }
            else {  // We desire metric Units (meters)
                if (!sdts_elevation_in_meters) {
                    // Map file is in feet, convert to meters for
                    // display
                    xastir_snprintf(label,
                        sizeof(label),
                        "%im",
                        (int)((OGR_F_GetFieldAsInteger(featureH, ii) * 0.3048) + 0.5));
                }
                else {
                    // Map file is already in meters
                    xastir_snprintf(label,
                        sizeof(label),
                        "%sm",
                        OGR_F_GetFieldAsString(featureH, ii));
                }
            }

            // Test for empty value
            if (label[0] == 'f' || label[0] == 'm')
                return;

//fprintf(stderr,"Elevation label: %s, angle: %2.1f\n", label, angle);
        }
    }


    // Check whether we found a field and if we haven't already
    // created a label.  If so, snag the field.
    if (ii != -1 && label[0] == '\0') {
        pii = OGR_F_GetFieldAsString(featureH, ii);

        if (pii && pii[0] != '\0') {
            xastir_snprintf(label,
                sizeof(label),
                "%s",
                pii);
        }
    }


    // Draw at least one label.  In the future we can pick and
    // choose among the points passed to us and draw the quantity of
    // labels that are appropriate for the zoom level.  Whether to
    // draw labels for more minor objects as we zoom out is another
    // problem that must be solved.
    //
    if (label && map_labels /* && !skip_label */ ) {

        // Check that we're not trying to draw the label out of
        // bounds for the X11 calls.
        //
        if (       xpoints[0].x+10 <  15000
                && xpoints[0].x+10 > -15000
                && xpoints[0].y+5  <  15000
                && xpoints[0].y+5  > -15000) {

/*
            if (angle == 0.0) {   // Non-rotated label

                draw_rotated_label_text (w,
                    -90.0,
                    xpoints[0].x+10,
                    xpoints[0].y+5,
                    strlen(label),
                    colors[color],
                    label,
                    FONT_DEFAULT);
            }

            else {  // Rotated label
*/
                draw_rotated_label_text (w,
                    angle,
                    xpoints[0].x+10,
                    xpoints[0].y+5,
                    strlen(label),
                    colors[color],
                    label,
                    FONT_DEFAULT);
//            }
        }
    }

//fprintf(stderr,"Draw_OGR_Labels end\n");

}





// Draw_OGR_Points().
//
// A function which can be recursively called.  Tracks the recursion
// depth so that we can recover if we exceed the maximum.  If we
// keep finding geometries below us, keep calling the same function.
// Simple and efficient.
//
// Really the important thing here is not to draw a tiny dot on the
// screen, but to draw a symbol and/or a label at that point on the
// screen.  We'll need more code to handle that stuff though, to
// determine which field in the file to use as a label and the
// font/color/placement/size/etc.
// 
void Draw_OGR_Points( Widget w,
                      OGRFeatureH featureH,
                      OGRGeometryH geometryH,
                      int level,
                      OGRCoordinateTransformationH transformH) {
 
    int kk;
    int object_num = 0;
    int num_points;
    int ii;


//fprintf(stderr, "Draw_OGR_Points\n");

    if (geometryH == NULL)
        return; // Exit early

    // Check for more objects below this one, recursing into any
    // objects found.  "level" keeps us from recursing too far (we
    // don't want infinite recursion here).
    // 
    object_num = OGR_G_GetGeometryCount(geometryH);
    // Iterate through the objects found.  If another geometry is
    // detected, call this function again recursively.  That will
    // cause all of the lower objects to get drawn.
    //
    if (object_num) {

        //fprintf(stderr, "DrawPoints: Found %d geometries\n", object_num);
 
        for ( kk = 0; kk < object_num; kk++ ) {
            OGRGeometryH child_geometryH;
            int sub_object_num;


            // See if we have geometries below this one.
            child_geometryH = OGR_G_GetGeometryRef(geometryH, kk);

            sub_object_num = OGR_G_GetGeometryCount(child_geometryH);

            if (sub_object_num) {
                // We found geometries below this.  Recurse.
                if (level < 5) {
                    //fprintf(stderr, "DrawPoints: Recursing level %d\n", level);
                    Draw_OGR_Points(w,
                        featureH,
                        child_geometryH,
                        level+1,
                        transformH);
                }
            }
        }
        return;
    }


    // Draw

    // Get number of elements (points)
    num_points = OGR_G_GetPointCount(geometryH);
    //fprintf(stderr,"  Number of elements: %d\n",num_points);

    // Draw the points
    for ( ii = 0; ii < num_points; ii++ ) {
        double X1, Y1, Z1;
        XPoint xpoint;


        // Get the point!
        OGR_G_GetPoint(geometryH,
            ii,
            &X1,
            &Y1,
            &Z1);

        if (transformH) {
            // Convert to WGS84 coordinates.
            if (!OCTTransform(transformH, 1, &X1, &Y1, &Z1)) {
                fprintf(stderr,
                    "Couldn't convert point to WGS84\n");
                // Draw it anyway.  It _might_ be in WGS84 or
                // NAD83!
            }
        }


        // Skip the map_visible_lat_lon() check:
        // draw_point_ll() does the check for us.
        //
        draw_point_ll(da,
            (float)Y1,
            (float)X1,
            gc,
            pixmap);

// We could use a flag back from draw_point_ll() that tells us
// whether the point was within the view.  That way we know whether
// or not to draw the label.

        xpoint.x = (short)X1;
        xpoint.y = (short)Y1;

/*
        // Draw the corresponding label
        Draw_OGR_Labels(w,
            pixmap,
            featureH,
            geometryH,
            &xpoint,
            1,   // Number of points
            label_color_guess);
*/
    }
}

 



// Draw_OGR_Lines().
//
// A function which can be recursively called.  Tracks the recursion
// depth so that we can recover if we exceed the maximum.  If we
// keep finding geometries below us, keep calling the same function.
// Simple and efficient.
//
// In the case of !fast_extents, it might be faster to convert the
// entire object to Xastir coordinates, then check whether it is
// visible.  That would elimate two conversions in the case that the
// object gets drawn.  For the fast_extents case, we're just
// snagging the extents instead of the entire object, so we might
// just leave it as-is.
//
void Draw_OGR_Lines( Widget w,
                     OGRFeatureH featureH,
                     OGRGeometryH geometryH,
                     int level,
                     OGRCoordinateTransformationH transformH,
                     int fast_extents) {
 
    int kk;
    int object_num = 0;
    int num_points;
    OGREnvelope envelopeH;


//fprintf(stderr, "Draw_OGR_Lines\n");

    if (geometryH == NULL)
        return; // Exit early

    // Check for more objects below this one, recursing into any
    // objects found.  "level" keeps us from recursing too far (we
    // don't want infinite recursion here).
    // 
    object_num = OGR_G_GetGeometryCount(geometryH);

    // Iterate through the objects found.  If another geometry is
    // detected, call this function again recursively.  That will
    // cause all of the lower objects to get drawn.
    //
    if (object_num) {

        //fprintf(stderr, "DrawLines: Found %d geometries\n", object_num);
 
        for ( kk = 0; kk < object_num; kk++ ) {
            OGRGeometryH child_geometryH;
            int sub_object_num;


            // Check for more geometries below this one.
            child_geometryH = OGR_G_GetGeometryRef(geometryH, kk);

            sub_object_num = OGR_G_GetGeometryCount(child_geometryH);

            if (sub_object_num) {
                // We found geometries below this.  Recurse.
                if (level < 5) {
                    //fprintf(stderr, "DrawLines: Recursing level %d\n", level);
                    Draw_OGR_Lines(w,
                        featureH,
                        child_geometryH,
                        level+1,
                        transformH,
                        fast_extents);
                }
            }
        }
        return;
    }


    // Draw it.  Standard linestring files return 0 for object_num.
    // Multilinestring must return more, so we recurse?
    // Yet to be tested!


    // If we have fast_extents available, take advantage of it to
    // escape this routine as quickly as possible, should the object
    // not be within our view.
    //
    if (fast_extents) {

        // Get the extents for this Polyline
        OGR_G_GetEnvelope(geometryH, &envelopeH);

        // Convert the extents to WGS84/Geographic coordinate system
        if (transformH) {
//fprintf(stderr,"Converting to WGS84\n");
            // Convert to WGS84 coordinates.
            if (!OCTTransform(transformH, 2, &envelopeH.MinX, &envelopeH.MinY, NULL)) {
                fprintf(stderr,
                    "Couldn't convert point to WGS84\n");
            }
        }

/*
fprintf(stderr,"MinY:%f, MaxY:%f, MinX:%f, MaxX:%f\n",
    envelopeH.MinY,
    envelopeH.MaxY,
    envelopeH.MinX,
    envelopeH.MaxX);
*/

        if (map_visible_lat_lon( envelopeH.MinY,    // bottom
                envelopeH.MaxY, // top
                envelopeH.MinX, // left
                envelopeH.MaxX, // right
                NULL)) {
//fprintf(stderr, "Fast Extents: Line is visible\n");
        }
        else {
//fprintf(stderr, "Fast Extents: Line is NOT visible\n");
            return; // Exit early
        }

        // If we made it this far with an object with
        // fast_extents , the feature is within our view.
    }

    else { // Fast extents are not available.  Compute the
           // extents ourselves once we have all the points,
           // then check whether this object is within our
           // view.
    }


    num_points = OGR_G_GetPointCount(geometryH);
//fprintf(stderr,"  Number of elements: %d\n",num_points);


    // Draw the polyline
    //
    if (num_points > 0) {
        int ii;
        double *vectorX;
        double *vectorY;
        double *vectorZ;
        unsigned long *XL = NULL;
        unsigned long *YL = NULL;
        long *XI = NULL;
        long *YI = NULL;
        XPoint *xpoints = NULL;


        // Get some memory to hold the vectors
        vectorX = (double *)malloc(sizeof(double) * num_points);
        vectorY = (double *)malloc(sizeof(double) * num_points);
        vectorZ = (double *)malloc(sizeof(double) * num_points);
        CHECKMALLOC(vectorX);
        CHECKMALLOC(vectorY);
        CHECKMALLOC(vectorZ);

        // Get the points, fill in the vectors
        for ( ii = 0; ii < num_points; ii++ ) {

            OGR_G_GetPoint(geometryH,
                ii,
                &vectorX[ii],
                &vectorY[ii],
                &vectorZ[ii]);
        }

        if (transformH) {
//fprintf(stderr,"Converting to WGS84\n");
            // Convert all vectors to WGS84 coordinates.
            if (!OCTTransform(transformH, num_points, vectorX, vectorY, vectorZ)) {
                fprintf(stderr,
                    "Couldn't convert vectors to WGS84\n");
            }
        }
        else {
//fprintf(stderr,"No transform available\n");
        }

        // For the non-fast_extents case, we need to compute the
        // extents and check whether this object is within our
        // view.
        //
        if (!fast_extents) {
            double MinX, MaxX, MinY, MaxY;

            MinX = vectorX[0];
            MaxX = vectorX[0];
            MinY = vectorY[0];
            MaxY = vectorY[0];

            for ( ii = 1; ii < num_points; ii++ ) {
                if (vectorX[ii] < MinX) MinX = vectorX[ii];
                if (vectorX[ii] > MaxX) MaxX = vectorX[ii];
                if (vectorY[ii] < MinY) MinY = vectorY[ii];
                if (vectorY[ii] > MaxY) MaxY = vectorY[ii];
            }

/*
fprintf(stderr,"MinY:%f, MaxY:%f, MinX:%f, MaxX:%f\n",
    MinY,
    MaxY,
    MinX,
    MaxX);
*/

            // We have the extents now in geographic/WGS84
            // datum.  Check for visibility.
            //
            if (map_visible_lat_lon( MinY,    // bottom
                    MaxY, // top
                    MinX, // left
                    MaxX, // right
                    NULL)) {
//fprintf(stderr, "Line is visible\n");
            }
            else {

//fprintf(stderr, "Line is NOT visible\n");

                // Free the allocated vector memory
                if (vectorX)
                    free(vectorX);
                if (vectorY)
                    free(vectorY);
                if (vectorZ)
                    free(vectorZ);

                return; // Exit early
            }
        }

        // If we've gotten to this point, part or all of the
        // object is within our view so at least some drawing
        // should occur.

        XL = (unsigned long *)malloc(sizeof(unsigned long) * num_points);
        YL = (unsigned long *)malloc(sizeof(unsigned long) * num_points);
        CHECKMALLOC(XL);
        CHECKMALLOC(YL);

        // Convert arrays to the Xastir coordinate system
        for (ii = 0; ii < num_points; ii++) {
            convert_to_xastir_coordinates(&XL[ii],
                &YL[ii],
                vectorX[ii],
                vectorY[ii]);
        }

        // Free the allocated vector memory
        if (vectorX)
            free(vectorX);
        if (vectorY)
            free(vectorY);
        if (vectorZ)
            free(vectorZ);

        XI = (long *)malloc(sizeof(long) * num_points);
        YI = (long *)malloc(sizeof(long) * num_points);
        CHECKMALLOC(XI);
        CHECKMALLOC(YI);

        // Convert arrays to screen coordinates.  Careful here!
        // The format conversions you'll need if you try to
        // compress this into two lines will get you into
        // trouble.
        //
        // We also clip to screen size and compute min/max
        // values here.
        for (ii = 0; ii < num_points; ii++) {
            XI[ii] = XL[ii] - x_long_offset;
            XI[ii] = XI[ii] / scale_x;

            YI[ii] = YL[ii] - y_lat_offset;
            YI[ii] = YI[ii] / scale_y;

// Here we truncate:  We should polygon clip instead, so that the
// slopes of the line segments don't change.  Points beyond +/-
// 16000 can cause problems in X11 when we draw.
            if      (XI[ii] >  15000l) XI[ii] =  15000l;
            else if (XI[ii] < -15000l) XI[ii] = -15000l;
            if      (YI[ii] >  15000l) YI[ii] =  15000l;
            else if (YI[ii] < -15000l) YI[ii] = -15000l;
        }

        // We don't need the Xastir coordinate system arrays
        // anymore.  We've already converted to screen
        // coordinates.
        if (XL)
            free(XL);
        if (YL)
            free(YL);

        // Set up the XPoint array.
        xpoints = (XPoint *)malloc(sizeof(XPoint) * num_points);
        CHECKMALLOC(xpoints);

        // Load up our xpoints array
        for (ii = 0; ii < num_points; ii++) {
            xpoints[ii].x = (short)XI[ii];
            xpoints[ii].y = (short)YI[ii];
        }

        // Free the screen coordinate arrays.
        if (XI)
            free(XI);
        if (YI)
            free(YI);


        // Actually draw the lines
        (void)XDrawLines(XtDisplay(da),
            pixmap,
            gc,
            xpoints,
            num_points,
            CoordModeOrigin);

        // Draw the corresponding labels
        Draw_OGR_Labels(w,
            pixmap,
            featureH,
            geometryH,
            xpoints,
            num_points,
            label_color_guess);

        if (xpoints)
            free(xpoints);

    }
}





// create_clip_mask()
//
// Create a rectangular X11 Region.  It should be the size of the
// extents for the outer polygon.
//
Region create_clip_mask( int num,
                         int minX,
                         int minY,
                         int maxX,
                         int maxY) {

    XRectangle rectangle;
    Region region;


//fprintf(stderr,"create_clip_mask: num=%i ", num);

    rectangle.x      = (short) minX;
    rectangle.y      = (short) minY;
    rectangle.width  = (unsigned short)(maxX - minX + 1);
    rectangle.height = (unsigned short)(maxY - minY + 1);

    // Create an empty region
    region = XCreateRegion();

/*
    fprintf(stderr,"Create: x:%d y:%d x:%d y:%d\n",
        minX,
        minY,
        maxX,
        maxY);

    fprintf(stderr,"Create: x:%d y:%d w:%d h:%d\n",
        rectangle.x,
        rectangle.y,
        rectangle.width,
        rectangle.height);
*/

    // Create a region containing a filled rectangle
    XUnionRectWithRegion(&rectangle,
        region,
        region);

//fprintf(stderr,"Done\n");

    return(region);
}


 


// create_hole_in_mask()
//
// Create a hole in an X11 Region, using a polygon as input.  X11
// Region must have been created with "create_clip_mask()" before
// this function is called.
//
Region create_hole_in_mask( Region mask,
                            int num,
                            long *X,
                            long *Y) {

    Region region2 = NULL;
    Region region3 = NULL;
    XPoint *xpoints = NULL;
    int ii;


//fprintf(stderr,"create_hole_in_mask: num=%i ", num);

    if (num < 3) {  // Not enough for a polygon
        fprintf(stderr,
            "create_hole_in_mask:XPolygonRegion w/too few vertices: %d\n",
            num);
        return(mask);    // Net result = no change to Region
    }

    // Get memory to hold the points
    xpoints = (XPoint *)malloc(sizeof(XPoint) * num);
    CHECKMALLOC(xpoints);

    // Load up our xpoints array
    for (ii = 0; ii < num; ii++) {
        xpoints[ii].x = (short)X[ii];
        xpoints[ii].y = (short)Y[ii];
    }

    // Create empty regions
    region2 = XCreateRegion();
    region3 = XCreateRegion();

    // Draw the "hole" polygon
    region2 = XPolygonRegion(xpoints,
        num,
        WindingRule);

    // Free the allocated memory
    if (xpoints)
        free(xpoints);

    XSubtractRegion(mask, region2, region3);

    // Get rid of the two regions we no longer need.
    XDestroyRegion(mask);
    XDestroyRegion(region2);

//fprintf(stderr,"Done\n");

    // Return our new region that has another hole in it.
    return(region3);
}





// draw_polygon_with_mask()
//
// Draws a polygon onto a pixmap using an X11 Region to mask areas
// that shouldn't be drawn over (holes).  X11 Region is created with
// the "create_clip_mask()" function, holes in it are created with
// the "create_hole_in_mask()" function.  The polygon used to create
// the initial X11 Region was saved away during the creation.  Now
// we just draw it to the pixmap using the X11 Region as a mask.
//
void draw_polygon_with_mask( Region mask,
                             XPoint *xpoints,
                             int num_points) {

    GC gc_temp = NULL;
    XGCValues gc_temp_values;


//    fprintf(stderr,"draw_polygon_with_mask: ");

    // There were "hole" polygons, so by now we've created a "holey"
    // region.  Draw a filled polygon with gc_temp here and then get
    // rid of gc_temp and the regions.

    gc_temp = XCreateGC(XtDisplay(da),
        XtWindow(da),
        0,
        &gc_temp_values);


    // Set the clip-mask into the GC.  This GC is now ruined for
    // other purposes, so destroy it when we're done drawing this
    // one shape.
    if (mask != NULL)
        XSetRegion(XtDisplay(da), gc_temp, mask);

    (void)XSetForeground(XtDisplay(da),
        gc_temp,
        colors[(int)label_color_guess]);

    // Actually draw the filled polygon
    (void)XFillPolygon(XtDisplay(da),
        pixmap,
        gc_temp,
        xpoints,
        num_points,
        Nonconvex,
        CoordModeOrigin);

    if (mask != NULL)
        XDestroyRegion(mask);

    if (gc_temp != NULL)
        XFreeGC(XtDisplay(da), gc_temp);

//    fprintf(stderr,"Done!\n");
}





// Draw_OGR_Polygons().
//
// A function which can be recursively called.  Tracks the recursion
// depth so that we can recover if we exceed the maximum.  If we
// keep finding geometries below us, keep calling the same function.
// Simple and efficient.
// 
// This can get complicated for Shapefiles:  Polygons are composed
// of multiple rings.  If a ring goes in one direction, it's a fill
// polygon, if the other direction, it's a hole polygon.
//
// Can test the operation using Shapefiles for Island County, WA,
// Camano Island, where there's an Island in Puget Sound that has a
// lake with an island in it (48.15569N/122.48953W)!
//
// At 48.43867N/122.61687W there's another lake with an island in
// it, but this lake isn't on an island like the first example.
//
// A note from Frank Warmerdam (GDAL guy):
//
// "An OGRPolygon objects consists of one outer ring and zero or
// more inner rings.  The inner rings are holes.  The C API kind of
// hides this fact, but you should be able to assume that the first
// ring is an outer ring, and any other rings are inner rings
// (holes).  Note that in the simple features geometry model you
// can't make any assumptions about the winding direction of holes
// or outer rings.
//
// Some polygons when read from Shapefiles will be returned as
// OGRMultiPolygon.  These are basically areas that have multiple
// outer rings.  For instance, if you had one feature that was a
// chain of islands.  In the past (and likely even now with some
// format drivers) these were just returned as polygons with the
// outer and inner rings all mixed up.  But the development code
// includes fixes (for shapefiles at least) to fix this up and
// convert into a multi polygon so you can properly establish what
// are outer rings, and what are inner rings (holes)."
//
// In the case of !fast_extents, it might be faster to convert the
// entire object to Xastir coordinates, then check whether it is
// visible.  That would elimate two conversions in the case that the
// object gets drawn.  For the fast_extents case, we're just
// snagging the extents instead of the entire object, so we might
// just leave it as-is.
//
void Draw_OGR_Polygons( Widget w,
                        OGRFeatureH featureH,
                        OGRGeometryH geometryH,
                        int level,
                        OGRCoordinateTransformationH transformH,
                        int draw_filled,
                        int fast_extents) {
 
    int kk;
    int object_num = 0;
    Region mask = NULL;
    XPoint *xpoints = NULL;
    int num_outer_points = 0;


//fprintf(stderr,"Draw_OGR_Polygons\n");

    if (geometryH == NULL)
        return; // Exit early

    // Check for more objects below this one, recursing into any
    // objects found.  "level" keeps us from recursing too far (we
    // don't want infinite recursion here).  These objects may be
    // rings or they may be other polygons in a collection.
    // 
    object_num = OGR_G_GetGeometryCount(geometryH);

    // Iterate through the objects found.  If another geometry is
    // detected, call this function again recursively.  That will
    // cause all of the lower objects to get drawn.
    //
    for ( kk = 0; kk < object_num; kk++ ) {
        OGRGeometryH child_geometryH;
        int sub_object_num;


        // This may be a ring, or another object with rings.
        child_geometryH = OGR_G_GetGeometryRef(geometryH, kk);

        sub_object_num = OGR_G_GetGeometryCount(child_geometryH);

        if (sub_object_num) {
            // We found geometries below this.  Recurse.
            if (level < 5) {

                // If we got here, we're dealing with a multipolygon
                // file.  There are multiple sets of polygon
                // objects, each may have inner (hole) and outer
                // (fill) rings.  If (level > 0), it's a
                // multipolygon layer.

//fprintf(stderr, "DrawPolygons: Recursing level %d\n", level);

                Draw_OGR_Polygons(w,
                    featureH,
                    child_geometryH,
                    level+1,
                    transformH,
                    draw_filled,
                    fast_extents);
            }
        }

        else {  // Draw
            int polygon_points;
            OGREnvelope envelopeH;
            int polygon_hole = 0;
            int single_polygon = 0;


            // if (kk==0) we're dealing with an outer (fill) ring.
            // If (kk>0) we're dealing with an inner (hole) ring.
            if (kk == 0 || object_num == 1) {
                if (object_num == 1) {
//fprintf(stderr,"Polygon->Fill\n");
                    single_polygon++;
                }
                else {
//fprintf(stderr,"Polygon->Fill w/holes\n");
                }
                
            }
            else if (object_num > 1) {
//fprintf(stderr,"Polygon->Hole\n");
                polygon_hole++;
            }

            if (fast_extents) {

                // Get the extents for this Polygon
                OGR_G_GetEnvelope(geometryH, &envelopeH);

                // Convert them to WGS84/Geographic coordinate system
                if (transformH) {
                    // Convert to WGS84 coordinates.
                    if (!OCTTransform(transformH, 2, &envelopeH.MinX, &envelopeH.MinY, NULL)) {
                        fprintf(stderr,
                            "Couldn't convert to WGS84\n");
                    }
                }

                if (map_visible_lat_lon( envelopeH.MinY,    // bottom
                        envelopeH.MaxY, // top
                        envelopeH.MinX, // left
                        envelopeH.MaxX, // right
                        NULL)) {
                    //fprintf(stderr, "Fast Extents: Polygon is visible\n");
                }
                else {
                    //fprintf(stderr, "Fast Extents: Polygon is NOT visible\n");
                    return; // Exit early
                }

                // If we made it this far, the feature is within our
                // view (if it has fast_extents).
            }
            else {
                // Fast extents not available.  We'll need to
                // compute our own extents below.
            }

            polygon_points = OGR_G_GetPointCount(child_geometryH);
//fprintf(stderr, "Vertices: %d\n", polygon_points);

            if (polygon_points > 3) { // Not a polygon if < 3 points
                int mm;
                double *vectorX;
                double *vectorY;
                double *vectorZ;


                // Get some memory to hold the polygon vectors
                vectorX = (double *)malloc(sizeof(double) * polygon_points);
                vectorY = (double *)malloc(sizeof(double) * polygon_points);
                vectorZ = (double *)malloc(sizeof(double) * polygon_points);
                CHECKMALLOC(vectorX);
                CHECKMALLOC(vectorY);
                CHECKMALLOC(vectorZ);

                // Get the points, fill in the vectors
                for ( mm = 0; mm < polygon_points; mm++ ) {

                    OGR_G_GetPoint(child_geometryH,
                        mm,
                        &vectorX[mm],
                        &vectorY[mm],
                        &vectorZ[mm]);
                }

 
                if (transformH) {
                    // Convert entire polygon to WGS84 coordinates.
                    if (!OCTTransform(transformH, polygon_points, vectorX, vectorY, vectorZ)) {
//                        fprintf(stderr,
//                            "Couldn't convert polygon to WGS84\n");
//return;
                    }
                }

 
                // For the non-fast_extents case, we need to compute
                // the extents and check whether this object is
                // within our view.
                //
                if (!fast_extents) {
                    double MinX, MaxX, MinY, MaxY;

                    MinX = vectorX[0];
                    MaxX = vectorX[0];
                    MinY = vectorY[0];
                    MaxY = vectorY[0];

                    for ( mm = 1; mm < polygon_points; mm++ ) {
                        if (vectorX[mm] < MinX) MinX = vectorX[mm];
                        if (vectorX[mm] > MaxX) MaxX = vectorX[mm];
                        if (vectorY[mm] < MinY) MinY = vectorY[mm];
                        if (vectorY[mm] > MaxY) MaxY = vectorY[mm];
                    }

                    // We have the extents now in geographic/WGS84
                    // datum.  Check for visibility.
                    //
                    if (map_visible_lat_lon( MinY,    // bottom
                            MaxY, // top
                            MinX, // left
                            MaxX, // right
                            NULL)) {
//fprintf(stderr, "Polygon is visible\n");
                    }
                    else {

//fprintf(stderr, "Polygon is NOT visible\n");

                        // Free the allocated vector memory
                        if (vectorX)
                            free(vectorX);
                        if (vectorY)
                            free(vectorY);
                        if (vectorZ)
                            free(vectorZ);

                        //return; // Exit early
                        continue;   // Next ieration of the loop
                    }
                }


                // If draw_filled != 0, draw the polygon using X11
                // polygon calls instead of just drawing the border.
                //
                if (draw_filled) { // Draw a filled polygon
                    unsigned long *XL = NULL;
                    unsigned long *YL = NULL;
                    long *XI = NULL;
                    long *YI = NULL;
                    int nn;
                    int minX, maxX, minY, maxY;


                    XL = (unsigned long *)malloc(sizeof(unsigned long) * polygon_points);
                    YL = (unsigned long *)malloc(sizeof(unsigned long) * polygon_points);
                    CHECKMALLOC(XL);
                    CHECKMALLOC(YL);
 
                    // Convert arrays to the Xastir coordinate
                    // system
                    for (nn = 0; nn < polygon_points; nn++) {
                        convert_to_xastir_coordinates(&XL[nn],
                            &YL[nn],
                            vectorX[nn],
                            vectorY[nn]);
                    }


                    XI = (long *)malloc(sizeof(long) * polygon_points);
                    YI = (long *)malloc(sizeof(long) * polygon_points);
                    CHECKMALLOC(XI);
                    CHECKMALLOC(YI);
 
// Note:  We're limiting screen size to 15000 in this routine.
                    minX = -15000;
                    maxX =  15000;
                    minY = -15000;
                    maxY =  15000;
 

                    // Convert arrays to screen coordinates.
                    // Careful here!  The format conversions you'll
                    // need if you try to compress this into two
                    // lines will get you into trouble.
                    //
                    // We also clip to screen size and compute
                    // min/max values here.
                    for (nn = 0; nn < polygon_points; nn++) {
                        XI[nn] = XL[nn] - x_long_offset;
                        XI[nn] = XI[nn] / scale_x;

                        YI[nn] = YL[nn] - y_lat_offset;
                        YI[nn] = YI[nn] / scale_y;
 
// Here we truncate:  We should polygon clip instead, so that the
// slopes of the line segments don't change.  Points beyond +/-
// 16000 can cause problems in X11 when we draw.  Here we are more
// interested in keeping the rectangles small and fast.  Screen-size
// or smaller basically.
                        if      (XI[nn] >  15000l) XI[nn] =  15000l;
                        else if (XI[nn] < -15000l) XI[nn] = -15000l;
                        if      (YI[nn] >  15000l) YI[nn] =  15000l;
                        else if (YI[nn] < -15000l) YI[nn] = -15000l;

                        if (!polygon_hole) {

                            // Find the min/max extents for the
                            // arrays.  We use that to set the size
                            // of our mask region.
                            if (XI[nn] < minX) minX = XI[nn];
                            if (XI[nn] > maxX) maxX = XI[nn];
                            if (YI[nn] < minY) minY = YI[nn];
                            if (YI[nn] > maxY) maxY = YI[nn];
                        }
                    }


                    // We don't need the Xastir coordinate system
                    // arrays anymore.  We've already converted to
                    // screen coordinates.
                    if (XL)
                        free(XL);
                    if (YL)
                        free(YL);

                    if (!polygon_hole) {    // Outer ring (fill)
                        int pp;

                        // Set up the XPoint array that we'll need
                        // for our final draw (after the "holey"
                        // region is set up if we have multiple
                        // rings).
                        xpoints = (XPoint *)malloc(sizeof(XPoint) * polygon_points);
                        CHECKMALLOC(xpoints);

                        // Load up our xpoints array
                        for (pp = 0; pp < polygon_points; pp++) {
                            xpoints[pp].x = (short)XI[pp];
                            xpoints[pp].y = (short)YI[pp];
                        }
                        num_outer_points = polygon_points;
                    }


                    // If we have no inner polygons, skip the whole
                    // X11 Region thing and just draw a filled
                    // polygon to the pixmap for speed.  We do that
                    // here via the "single_polygon" variable.
                    //
                    if (!polygon_hole) {   // Outer ring (fill)

                        if (single_polygon) {
                            mask = NULL;
                        }
                        else {
                            // Pass the extents of the polygon to
                            // create a mask rectangle out of them.
                            mask = create_clip_mask(polygon_points,
                                minX,
                                minY,
                                maxX,
                                maxY);
                        }
                    }
                    else {  // Inner ring (hole)

                        // Pass the entire "hole" polygon set of
                        // vertices into the hole region creation
                        // function.  This knocks a hole in our mask
                        // so that underlying map layers can show
                        // through.

                        // This segfaults for TIGER polygons without
                        // the if(mask) part.
                        //
                        if (mask) { // create_clip_mask has been
                                    // called already
                            mask = create_hole_in_mask(mask,
                                polygon_points,
                                XI,
                                YI);
                        }
                        else {
                            fprintf(stderr,
                                "Draw_OGR_Polygons: Attempt to make hole in empty polygon mask!\n");
                        }
                    }


                    // Free the screen coordinate arrays.
                    if (XI)
                        free(XI);
                    if (YI)
                        free(YI);

                    // Draw the original polygon to the pixmap
                    // using the X11 Region as a mask.  Mask may be
                    // null if we're doing a single outer polygon
                    // with no "hole" polygons.
                    if (kk == (object_num - 1)) {
                        draw_polygon_with_mask(mask,
                            xpoints,
                            num_outer_points);


                        // Draw the corresponding labels
                        Draw_OGR_Labels(w,
                            pixmap,
                            featureH,
                            geometryH,
                            xpoints,
                            num_outer_points,
                            label_color_guess);

                        free(xpoints);
                    }
                }   // end of draw_filled


                else {  // We're drawing non-filled polygons.
                        // Draw just the border.

                    if (polygon_hole) {
                        // Inner ring, draw a dashed line
                        (void)XSetLineAttributes (XtDisplay(da), gc, 0, LineOnOffDash, CapButt,JoinMiter);
                    }
                    else {
                        // Outer ring, draw a solid line
                        (void)XSetLineAttributes (XtDisplay(da), gc, 0, LineSolid, CapButt,JoinMiter);
                    }

                    for ( mm = 1; mm < polygon_points; mm++ ) {

/*
fprintf(stderr,"Vector %d: %7.5f %8.5f  %7.5f %8.5f\n",
    mm,
    vectorY[mm-1],
    vectorX[mm-1],
    vectorY[mm],
    vectorX[mm]);
*/


                        draw_vector_ll(da,
                            (float)vectorY[mm-1],
                            (float)vectorX[mm-1],
                            (float)vectorY[mm],
                            (float)vectorX[mm],
                            gc,
                            pixmap);
                    }

/*
                    // Draw the corresponding labels
                    Draw_OGR_Labels(w,
                        pixmap,
                        featureH,
                        geometryH,
                        xpoints,
                        num_points,
                        label_color_guess);
*/

                }


// For weather polygons, we might want to draw the border color in a
// different color so that we can see these borders easily, or skip
// drawing the border itself for a few pixels, like UI-View does.

                // Free the allocated vector memory
                if (vectorX)
                    free(vectorX);
                if (vectorY)
                    free(vectorY);
                if (vectorZ)
                    free(vectorZ);
 
            }
            else {
                // Not enough points to draw a polygon
            }
        }
    }
}





// Set up coordinate translation:  We need it for indexing and
// drawing so we do it first and keep pointers to our transforms.
//
// Inputs:  datasourceH
//          layerH
//
// Outputs: map_spatialH
//          transformH
//          reverse_transformH
//          wgs84_spatialH
//          no_spatial
//          geographic
//          projected
//          local
//          datum
//          geogcs
//
void setup_coord_translation(OGRDataSourceH datasourceH,
        OGRLayerH layerH,
        OGRSpatialReferenceH *map_spatialH,
        OGRCoordinateTransformationH *transformH,
        OGRCoordinateTransformationH *reverse_transformH,
        OGRSpatialReferenceH *wgs84_spatialH,
        int *no_spatial,
        int *geographic,
        int *projected,
        int *local,
        const char *datum,
        const char *geogcs) {


    *map_spatialH = NULL;
    *transformH = NULL;
    *reverse_transformH = NULL;
    *wgs84_spatialH = NULL;
    datum = NULL;
    geogcs = NULL;


set_dangerous("map_gdal: OGR_DS_GetLayerCount");
    if (OGR_DS_GetLayerCount(datasourceH) <= 0) {
clear_dangerous();
        fprintf(stderr,"No layers detected\n");
        return; // No layers detected!
    }
clear_dangerous();


    // Query the coordinate system for the dataset.
set_dangerous("map_gdal: OGR_L_GetSpatialRef");
    *map_spatialH = OGR_L_GetSpatialRef(layerH);
clear_dangerous();


    if (*map_spatialH) {
        const char *temp;


        // We found spatial data
        no_spatial = 0;


set_dangerous("map_gdal: OGRIsGeographic");
        if (OSRIsGeographic(*map_spatialH)) {
            geographic++;
        }
        else if (OSRIsProjected(*map_spatialH)) {
            projected++;
        }
        else {
            local++;
        }
clear_dangerous();


        // PROJCS, GEOGCS, DATUM, SPHEROID, PROJECTION
        //
set_dangerous("map_gdal:OSRGetAttrValue");
        if (projected) {
            temp = OSRGetAttrValue(*map_spatialH, "PROJCS", 0);
            temp = OSRGetAttrValue(*map_spatialH, "PROJECTION", 0);
        }
        temp = OSRGetAttrValue(*map_spatialH, "SPHEROID", 0);
        datum = OSRGetAttrValue(*map_spatialH, "DATUM", 0);
        geogcs = OSRGetAttrValue(*map_spatialH, "GEOGCS", 0);
clear_dangerous();

    }


    else {

        if (debug_level & 16)
            fprintf(stderr,"  Couldn't get spatial reference\n");

        // For this case, assume that it is WGS84/geographic, and
        // attempt to index as-is.  If the numbers don't make sense,
        // we can skip indexing this dataset.

        // Perhaps some layers may have a spatial reference, and
        // others might not?  Solved this problem by defined
        // "no_spatial", which will be '1' if no spatial data was
        // found in any of the layers.  In that case we just store
        // the extents we find.
    }


//fprintf(stderr,"Datum: %s\n", datum);
//fprintf(stderr,"GOGCS: %s\n", geogcs);

 
    if (no_spatial) {   // No spatial info found

        *transformH = NULL;  // No transforms needed
        *reverse_transformH = NULL;

        if (debug_level & 16) {
            fprintf(stderr,
                "  No spatial info found, NO CONVERSION DONE!, %s\n",
                geogcs);
        }
    }
    else if ( geographic  // Geographic and correct datum
              && ( strcasecmp(geogcs,"WGS84") == 0
                || strcasecmp(geogcs,"NAD83") == 0
                || strcasecmp(datum,"North_American_Datum_1983") == 0
                || strcasecmp(datum,"World_Geodetic_System_1984") == 0
                || strcasecmp(datum,"D_North_American_1983") == 0 ) ) {

// We also need to check "DATUM", as some datasets have nothing in
// the "GEOGCS" variable.  Check for "North_American_Datum_1983" or
// "???".

        *transformH = NULL;  // No transforms needed
        *reverse_transformH = NULL;

        if (debug_level & 16) {
            fprintf(stderr,
                "  Geographic coordinate system, NO CONVERSION NEEDED!, %s\n",
                geogcs);
        }
    }

    else {  // We have coordinates but they're in the wrong datum or
            // in a projected/local coordinate system.  Set up a
            // transform to WGS84.

        if (geographic) {
            if (debug_level & 16) {
                fprintf(stderr,
                    "  Found geographic/wrong datum: %s.  Converting to wgs84 datum\n",
                    geogcs);
                fprintf(stderr, "  DATUM: %s\n", datum);
            }
        }

        else if (projected) {
            if (debug_level & 16) {
                fprintf(stderr,
                    "  Found projected coordinates: %s.  Converting to geographic/wgs84 datum\n",
                    geogcs);
            }
        }

        else if (local) {
            // Convert to geographic/WGS84?  How?

            if (debug_level & 16) {
                fprintf(stderr,
                    "  Found local coordinate system.  Returning\n");
            }

            // Close data source
            if (datasourceH != NULL) {
                OGR_DS_Destroy( datasourceH );
            }

            return; // Exit early
        }

        else {
            // Abandon all hope, ye who enter here!  We don't
            // have a geographic, projected, or local coordinate
            // system.

            // Close data source
            if (datasourceH != NULL) {
                OGR_DS_Destroy( datasourceH );
            }

            return; // Exit early
        }

 
        *wgs84_spatialH = OSRNewSpatialReference(NULL);


        if (*wgs84_spatialH == NULL) {
            if (debug_level & 16) {
                fprintf(stderr,
                    "Couldn't create empty wgs84_spatialH object\n");
            }

            // Close data source
            if (datasourceH != NULL) {
                OGR_DS_Destroy( datasourceH );
            }

            return; // Exit early
        }


        if (OSRSetWellKnownGeogCS(*wgs84_spatialH,"WGS84") == OGRERR_FAILURE) {
 
            // Couldn't fill in WGS84 parameters.
            if (debug_level & 16) {
                fprintf(stderr,
                    "Couldn't fill in wgs84 spatial reference parameters\n");
            }

            // NOTE: DO NOT destroy map_spatialH!  It is part of
            // the datasource.  You'll get a segfault if you
            // try, at the point where you destroy the
            // datasource.

            if (*wgs84_spatialH != NULL) {
                OSRDestroySpatialReference(*wgs84_spatialH);
            }

            // Close data source
            if (datasourceH != NULL) {
                OGR_DS_Destroy( datasourceH );
            }

            return; // Exit early
        }


        if (*map_spatialH == NULL || *wgs84_spatialH == NULL) {
            if (debug_level & 16) {
                fprintf(stderr,
                    "Couldn't transform because map_spatialH or wgs84_spatialH are NULL\n");
            }

            if (*wgs84_spatialH != NULL) {
                OSRDestroySpatialReference(wgs84_spatialH);
            }

            // Close data source
            if (datasourceH != NULL) {
                OGR_DS_Destroy( datasourceH );
            }

            return; // Exit early
        }


        else {
            // Set up transformation from original datum to wgs84
            // datum.
//fprintf(stderr,"Creating transform and reverse_transform\n");
            *transformH = OCTNewCoordinateTransformation(
                *map_spatialH, *wgs84_spatialH);

            // Set up transformation from wgs84 datum to original
            // datum.
            *reverse_transformH = OCTNewCoordinateTransformation(
                *wgs84_spatialH, *map_spatialH);


            if (*transformH == NULL || *reverse_transformH == NULL) {
                // Couldn't create transformation objects
//                if (debug_level & 16) {
                    fprintf(stderr,
                        "Couldn't create transformation objects\n");
//                }

/*
                if (*wgs84_spatialH != NULL) {
                    OSRDestroySpatialReference(wgs84_spatialH);
                }

                // Close data source
                if (datasourceH != NULL) {
                    OGR_DS_Destroy( datasourceH );
                }
*/

                return; // Exit early
            }
        }
    }

clear_dangerous();

}   // End of setup_coord_translation()





// Need this declaration for weather alerts currently
#ifdef HAVE_LIBSHP
extern void draw_shapefile_map (Widget w,
                                char *dir,
                                char *filenm,
                                alert_entry *alert,
                                u_char alert_color,
                                int destination_pixmap,
                                map_draw_flags *mdf);
#endif /* HAVE_LIBSHP */




// For TIGER, we must create polygons from the vector lines.  Use
// this function in the C API to do that:
//
//     OGRGeometryH CPL_DLL OGRBuildPolygonFromEdges(
//        OGRGeometryH hLinesAsCollection,
//        int bBestEffort,
//        int bAutoClose,
//        double dfTolerance,
//        OGRErr * peErr );
//
// We may need to create some as "hole" polygons and some as "fill"
// polygons, then order the vectors properly to pass off to the
// Draw_OGR_Polygon() function.
//
//
// The GDAL docs say to use these flags to compile:
// `gdal-config --libs` `gdal-config * --cflags`
// but so far they return: "-L/usr/local/lib -lgdal" and
// "-I/usr/local/include", which aren't much.  Leaving it hard-coded
// in configure.ac for now instead of using "gdal-config".
//
// GRASS module that does OGR (for reference):  v.in.ogr
//
// This function started out as very nearly verbatim the example
// C-API code off the OGR web pages.
//
// If destination_pixmap equals INDEX_CHECK_TIMESTAMPS or
// INDEX_NO_TIMESTAMPS, then we are indexing the file (finding the
// extents) instead of drawing it.
//
// Indexing/drawing works properly for either geographic or
// projected coordinates, and does conversions to geographic/WGS84
// datum before storing the extents in the map index.  It does not
// work for local coordinate systems.
//
//
// TODO:
// *) map_visible_ll() needs to be re-checked.  Lines are not
//    showing up sometimes when the end points are way outside the
//    view.  Perhaps need line_visible() and line_visible_ll()
//    functions?  If I don't want to compute the slope of a line and
//    such, might just check for line ends being within boundaries
//    or left/above, and within boundaries or right/below, to catch
//    those lines that slant across the view.
// *) Dbfawk support or similar?  Map preferences:  Colors, line
//    widths, layering, filled, choose label field, label
//    fonts/placement/color.
// *) Allow user to select layers to draw/ignore.  Very important
//    for those sorts of files that provide all layers (i.e. Tiger &
//    SDTS).
// *) Weather alert tinted polygons, draw to the correct pixmap,
//    draw only those shapes called for, not the entire map.
// *) Fast Extents:  We now pass a variable to the draw functions
//    that tells whether we can do fast extents, but we still need
//    to compute our own extents after we have the points for a
//    shape in an array.  We could either check extents or just call
//    the draw functions, which skip drawing if outside our view
//    anyway (we currently do the latter).


// We've changed over to the spatial filter scheme:  Set a spatial
// filter based on our current view, draw only those objects that
// OGR_L_GetNextFeature() passes back to us.  That saves a lot of
// coordinate conversion code and speeds things up dramatically.


// *) Figure out why SDTS hypsography (contour lines) on top of
//    terraserver gives strange results when zooming/panning
//    sometimes.  Restarting Xastir cleans up the image.  Perhaps
//    colors or GC's are getting messed up?  Perhaps lines segments
//    are too long when drawing?
// *) Some sort of warning to the operator if large regions are
//    being filled, and there's a base raster map?  Dbfawk likes to
//    fill counties in with purple, wiping out rasters underneath.
// *) Draw portions of a line/polygon correctly, instead of just
//    dropping or concatenating lines.  This can cause missing line
//    segments that cross the edge of our view, or incorrect slopes
//    for lines that cross the edge.
//    NOTE:  Have switched to chopping at +/- 15000 pixels, which
//    seems to fix this nicely.  Chopping at much less causes
//    problems, while X11 has problems at +/- 16384 or higher.
// *) Speed things up in any way possible.
//
// 
// Regarding coordinate systems, a note from Frank Warmerdam:  "In
// theory different layers in a datasource can have different
// coordinate systems, though this is uncommon.  It does happen
// though."
//
// Question regarding how to specify drawing preferences, answered
// by Frank:  "There are many standards.  Within the OGR API the
// preferred approach to this is the OGR Feature Style
// Specification:
//
// http://gdal.velocet.ca/~warmerda/projects/opengis/ogr_feature_style_008.html
//
// Basically this is intended to provide a mechanism for different
// underlying formats to indicate their rendering information.  It
// is at least partially implemented for the DGN and Mapinfo
// drivers.  The only applications that I am aware of taking
// advantage of it are OpenEV and MapServer."

void draw_ogr_map( Widget w,
                   char *dir,
                   char *filenm,
                   alert_entry *alert,
                   u_char alert_color,
                   int destination_pixmap,
                   map_draw_flags *mdf) {

    OGRDataSourceH datasourceH = NULL;
    OGRSFDriverH driver = NULL;
    OGRSpatialReferenceH map_spatialH = NULL;
    OGRSpatialReferenceH wgs84_spatialH = NULL;
    OGRCoordinateTransformationH transformH = NULL;
    OGRCoordinateTransformationH reverse_transformH = NULL;
    OGRGeometryH spatial_filter_geometryH = NULL;
    int ii, numLayers;
    char full_filename[300];
    const char *ptr;
    const char *driver_type;
    int no_spatial = 1;
    int geographic = 0;
    int projected = 0;
    int local = 0;
    const char *datum = NULL;
    const char *geogcs = NULL;
    double ViewX[2], ViewY[2], ViewZ[2];
    double ViewX2[2], ViewY2[2], ViewZ2[2];
    long ViewLX[2], ViewLY[2];
    float f_latitude0, f_latitude1, f_longitude0, f_longitude1;
    char status_text[MAX_FILENAME];
    char short_filenm[MAX_FILENAME];
    int draw_filled;

    draw_filled=mdf->draw_filled;
 
    if (debug_level & 16)
        fprintf(stderr,"Entered draw_ogr_map function\n");


/******************************************************************/
// We don't want to process weather alerts right now in this code.
// We're not set up for it yet.  Call the original shapefile map
// function instead.  We added an extern declaration above in order
// that this function knows about the draw_shapefile_map() function
// parameters.  Remove that declaration once we support weather
// alerts in this code natively.
//
if (alert) {

#ifdef HAVE_LIBSHP

    // We have a weather alert, call the original function instead.
    draw_shapefile_map(w,
        dir,
        filenm,
        alert,
        alert_color,
        destination_pixmap,
        mdf);

#endif  // HAVE_LIBSHP

    return;
}
/******************************************************************/


    xastir_snprintf(full_filename,
        sizeof(full_filename),
        "%s/%s",
        dir,
        filenm);


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


//    if (debug_level & 16)
        fprintf(stderr,"Opening datasource %s\n", full_filename);

    //
    // One computer segfaulted at OGROpen() if a .prj file was
    // present with a shapefile.  Another system with newer Linux/
    // libtiff/ libgeotiff works fine.  Getting rid of older header
    // files/ libraries, then recompiling/installing libproj/
    // libgeotiff/ libshape/ libgdal/ Xastir seemed to fix it.
    //
    // Open data source
set_dangerous("map_gdal: OGROpen");
    datasourceH = OGROpen(full_filename,
        0 /* bUpdate */,
        &driver);
clear_dangerous();

    if (datasourceH == NULL) {
/*
        fprintf(stderr,
            "Unable to open %s\n",
            full_filename);
*/
        return;
    }

    if (debug_level & 16)
        fprintf(stderr,"Opened datasource %s\n", full_filename);

    driver_type = OGR_Dr_GetName(driver);

    if (debug_level & 16)
        fprintf(stderr,"%s: ", driver_type);

    // Get name/path.  Less than useful since we should already know
    // this.
    ptr = OGR_DS_GetName(datasourceH);

    if (debug_level & 16)
        fprintf(stderr,"%s\n", ptr);


    // Implement the indexing functions, so that we can use these
    // map formats from within Xastir.  Without an index, it'll
    // never appear in the map chooser.  Use the OGR "envelope"
    // functions to get the extents for the each layer in turn.
    // We'll find the min/max of the overall set of layers and use
    // that for the extents for the entire dataset.
    //
    // Check whether we're indexing or drawing the map
    if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
            || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {


/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
// We're indexing only.  Save the extents in the index.
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////


        double file_MinX = 0;
        double file_MaxX = 0;
        double file_MinY = 0;
        double file_MaxY = 0;
        int first_extents = 1;


        xastir_snprintf(status_text,
            sizeof(status_text),
            langcode ("BBARSTA039"),
            short_filenm);
        statusline(status_text,0);       // Indexing ...

//fprintf(stderr,"Indexing %s\n", filenm);

        // Use the OGR "envelope" function to get the extents for
        // the entire file or dataset.  Remember that it could be in
        // state-plane coordinate system or something else equally
        // strange.  Convert it to WGS84 or NAD83 geographic
        // coordinates before saving the index.

        // Loop through all layers in the data source.  We can't get
        // the extents for the entire data source without looping
        // through the layers.
        //
        numLayers = OGR_DS_GetLayerCount(datasourceH);
        for ( ii=0; ii<numLayers; ii++ ) {
            OGRLayerH layerH;
            OGREnvelope psExtent;  


            layerH = OGR_DS_GetLayer( datasourceH, ii );

            if (layerH == NULL) {

                if (debug_level & 16) {
                    fprintf(stderr,
                        "Unable to open layer %d of %s\n",
                        ii,
                        full_filename);
                }

                if (wgs84_spatialH != NULL) {
                    OSRDestroySpatialReference(wgs84_spatialH);
                }

//                if (transformH != NULL) {
//                    OCTDestroyCoordinateTransformation(transformH);
//                }

//                if (reverse_transformH != NULL) {
//                    OCTDestroyCoordinateTransformation(reverse_transformH);
//                }

                // Close data source
                if (datasourceH != NULL) {
                    OGR_DS_Destroy( datasourceH );
                }

                return; // Exit early
            }


            // Set up the coordinate translations we may need for
            // both indexing and drawing operations.  It sets up the
            // transform and the reverse transforms we need to
            // convert between the spatial coordinate systems.
            //
            setup_coord_translation(datasourceH, // Input
                layerH,                          // Input
                &map_spatialH,                    // Output
                &transformH,                      // Output
                &reverse_transformH,              // Output
                &wgs84_spatialH,                  // Output
                &no_spatial,                     // Output
                &geographic,                     // Output
                &projected,                      // Output
                &local,                          // Output
                datum,                           // Output
                geogcs);                         // Output


            // Get the extents for this layer.  OGRERR_FAILURE means
            // that the layer has no spatial info or that it would be
            // an expensive operation to establish the extent.
            // Here we set the force option to TRUE in order to read
            // all of the extents even for files where that would be
            // an expensive operation.  We're trying to index the
            // file after all!  Whether or not the operation is
            // expensive makes no difference at this point.
            //
            if (OGR_L_GetExtent(layerH, &psExtent, TRUE) != OGRERR_FAILURE) {

//                fprintf(stderr,
//                    "  MinX: %f, MaxX: %f, MinY: %f, MaxY: %f\n",
//                    psExtent.MinX,
//                    psExtent.MaxX,
//                    psExtent.MinY,
//                    psExtent.MaxY);

                // If first value, store it.
                if (first_extents) {
                    file_MinX = psExtent.MinX;
                    file_MaxX = psExtent.MaxX;
                    file_MinY = psExtent.MinY;
                    file_MaxY = psExtent.MaxY;

                    first_extents = 0;
                }
                else {
                    // Store the min/max values
                    if (psExtent.MinX < file_MinX)
                        file_MinX = psExtent.MinX;
                    if (psExtent.MaxX > file_MaxX)
                        file_MaxX = psExtent.MaxX;
                    if (psExtent.MinY < file_MinY)
                        file_MinY = psExtent.MinY;
                    if (psExtent.MaxY > file_MaxY)
                        file_MaxY = psExtent.MaxY;
                }
            }
            // No need to free layerH handle, it belongs to the
            // datasource.
        }
        // All done looping through the layers.


        // We only know how to handle geographic or projected
        // coordinate systems.  Test for these.
        if ( !first_extents && (geographic || projected || no_spatial) ) {
            // Need to also check datum!  Must be NAD83 or WGS84 and
            // geographic for our purposes.
/*
            if ( no_spatial
                || (geographic
                    && ( strcasecmp(geogcs,"WGS84") == 0
                        || strcasecmp(geogcs,"NAD83") == 0) ) ) {
// Also check "datum" here

                fprintf(stderr,
                    "Geographic coordinate system, %s, adding to index\n",
                    geogcs);

                if (   file_MinY >=  -90.0 && file_MinY <=  90.0
                    && file_MaxY >=  -90.0 && file_MaxY <=  90.0
                    && file_MinX >= -180.0 && file_MinX <= 180.0
                    && file_MaxX >= -180.0 && file_MaxX <= 180.0) {

                    // Check for all-zero entries
                    if (       file_MinY == 0.0
                            && file_MaxY == 0.0
                            && file_MinX == 0.0
                            && file_MaxX == 0.0 ) {
//                        if (debug_level & 16) {
                            fprintf(stderr,
                                "Geographic coordinates are all zero, skipping indexing\n");
                            fprintf(stderr,"MinX:%f  MinY:%f  MaxX:%f MaxY:%f\n",
                                file_MinX,
                                file_MinY,
                                file_MaxX,
                                file_MaxY);
//                        }
                    }
                    else {
 
// Debug:  Don't add them to the index so that we can experiment
// with datum translation and such.
//#define WE7U
#ifndef WE7U
                        index_update_ll(filenm,    // Filename only
                            file_MinY,  // Bottom
                            file_MaxY,  // Top
                            file_MinX,  // Left
                            file_MaxX,  // Right
                            1000);      // Default Map Layer
#endif  // WE7U
                    }
                }
                else {
//                    if (debug_level & 16) {
                        fprintf(stderr,
                            "Geographic coordinates out of bounds, skipping indexing\n");
                        fprintf(stderr,"MinX:%f  MinY:%f  MaxX:%f MaxY:%f\n",
                            file_MinX,
                            file_MinY,
                            file_MaxX,
                            file_MaxY);
//                    }
                }
            }
            else {  // We have coordinates but they're in the wrong
                    // datum or in a projected coordinate system.
                    // Convert to WGS84 coordinates.
*/
                double x[2];
                double y[2];


                x[0] = file_MinX;
                x[1] = file_MaxX;
                y[0] = file_MinY;
                y[1] = file_MaxY;

//                fprintf(stderr,"Before: %f,%f\t%f,%f\n",
//                    x[0],y[0],
//                    x[1],y[1]);

                // We can get files that have a weird coordinate
                // system in them that doesn't have a transform
                // defined.  One such was "unamed".  Check whether
                // we have a valid transform.  If not, just assume
                // we're ok and index it as-is.
                if (transformH == NULL) {
                    if (debug_level & 16)
                        fprintf(stderr, "No transform available!\n");
                }
                else {

//                    fprintf(stderr,"Before: %f,%f\t%f,%f\n",
//                        x[0],y[0],
//                        x[1],y[1]);


                    if (OCTTransform(transformH, 2, x, y, NULL)) {
                            
//                        fprintf(stderr," After: %f,%f\t%f,%f\n",
//                            x[0],y[0],
//                            x[1],y[1]);
                    }
                }

                if (       y[0] >=  -90.0 && y[0] <=  90.0
                        && y[1] >=  -90.0 && y[1] <=  90.0
                        && x[0] >= -180.0 && x[0] <= 180.0
                        && x[1] >= -180.0 && x[1] <= 180.0) {

                    // Check for all-zero entries
                    if (       file_MinY == 0.0
                            && file_MaxY == 0.0
                            && file_MinX == 0.0
                            && file_MaxX == 0.0 ) {
//                        if (debug_level & 16) {
                            fprintf(stderr,
                                "Coordinates are all zero, skipping indexing\n");
                            fprintf(stderr,"MinX:%f  MinY:%f  MaxX:%f MaxY:%f\n",
                                file_MinX,
                                file_MinY,
                                file_MaxX,
                                file_MaxY);
//                        }
                    }
                    else {
 
// Debug:  Don't add them to the index so that we can experiment
// with datum translation and such.
#ifndef WE7U
                        index_update_ll(filenm, // Filename only
                            y[0],   // Bottom
                            y[1],   // Top
                            x[0],   // Left
                            x[1],   // Right
                            1000);  // Default Map Layer
#endif  // WE7U
                    }
                }
                else {
//                    if (debug_level & 16) {
                        fprintf(stderr,
                            "Coordinates out of bounds, skipping indexing\n");
                        fprintf(stderr,"MinX:%f  MinY:%f  MaxX:%f MaxY:%f\n",
                            file_MinX,
                            file_MinY,
                            file_MaxX,
                            file_MaxY);
//                    }
                }
//            }
        }

        // Debug code:
        // For now, set the index to be the entire world to get
        // things up and running.
//        index_update_ll(filenm,    // Filename only
//             -90.0,  // Bottom
//              90.0,  // Top
//            -180.0,  // Left
//             180.0,  // Right
//              1000); // Default Map Layer

        if (wgs84_spatialH != NULL) {
            OSRDestroySpatialReference(wgs84_spatialH);
        }

        if (transformH != NULL) {
            OCTDestroyCoordinateTransformation(transformH);
        }

        if (reverse_transformH != NULL) {
            OCTDestroyCoordinateTransformation(reverse_transformH);
        }

        // Close data source
        if (datasourceH != NULL) {
            OGR_DS_Destroy( datasourceH );
        }

        return; // Done indexing the file
    }


/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
// The code below this point is for drawing, not indexing.
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////


    xastir_snprintf(status_text,
        sizeof(status_text),
        langcode ("BBARSTA028"),
        short_filenm);
    statusline(status_text,0);       // Loading ...

//    fprintf(stderr,"Loading %s\n", filenm);


 
    // Find out what type of file we're dealing with.  This reports
    // one of:
    //
    // "AVCbin"
    // "DGN"
    // "FMEObjects Gateway"
    // "GML"
    // "Memory"
    // "MapInfo File"
    // "UK .NTF"
    // "OCI"
    // "ODBC"
    // "OGDI"
    // "PostgreSQL"
    // "REC"
    // "S57"
    // "SDTS"
    // "ESRI Shapefile"
    // "TIGER"
    // "VRT"
    //


    // If we're going to write, we need to test the capability using
    // these functions:
    // OGR_Dr_TestCapability(); // Does Driver have write capability?
    // OGR_DS_TestCapability(); // Can we create new layers?


// Optimization:  Get the envelope for each layer if it's not an
// expensive operation.  Skip the layer if it's completely outside
// our viewport.
// Faster yet:  Set up a SpatialFilter, then GetNextFeature() should
// only return features that are within our maximum bounding
// rectangle (MBR).  setup_coord_translation() may have already set
// up "wgs84_spatialH" for us as a SpatialReference.
 


//(void)XSetFunction();
(void)XSetFillStyle(XtDisplay(w), gc, FillSolid);



    // Set up the geometry for the spatial filtering we'll do later
    // in the layer loop.  The geometry should specify our current
    // view, but in the spatial reference system of the map layer.
    //
    // Fill in the corner coordinates of our viewport, in lat/long
    // coordinates.
    //
/*
    ViewX[0] = -110.0;
    ViewY[0] =   40.0;
    ViewZ[0] =    0.0;
    ViewX[1] = -090.0;
    ViewY[1] =   35.0;
    ViewZ[1] =    0.0;
*/
    //long  mid_x_long_offset;    // Longitude at center of map
    //long  mid_y_lat_offset;     // Latitude  at center of map

    // Longitude, NW corner of map screen
    ViewLX[0] = x_long_offset;
    // Latitude, NW corner of map screen
    ViewLY[0] = y_lat_offset;
    // Longitude, SE corner of map screen
    ViewLX[1] = mid_x_long_offset + (mid_x_long_offset - x_long_offset);
    // Latitude, SE corner of map screen
    ViewLY[1] = mid_y_lat_offset + (mid_y_lat_offset - y_lat_offset);

    // Check for out-of bounds values
    if (ViewLX[0] < 0) ViewLX[0] = 0;
    if (ViewLY[0] < 0) ViewLY[0] = 0;
    if (ViewLX[1] < 0) ViewLX[1] = 0;
    if (ViewLY[1] < 0) ViewLY[1] = 0;

    if (ViewLX[0] > 129600000) ViewLX[0] = 129600000;
    if (ViewLY[0] >  64800000) ViewLY[0] =  64800000;
    if (ViewLX[1] > 129600000) ViewLX[1] = 129600000;
    if (ViewLY[1] >  64800000) ViewLY[1] =  64800000;

    // Now convert the coordinates from Xastir coordinate system to
    // Lat/long degrees in the form of floats.

    convert_from_xastir_coordinates(&f_longitude0,
        &f_latitude0,
        ViewLX[0],
        ViewLY[0]);
    convert_from_xastir_coordinates(&f_longitude1,
        &f_latitude1,
        ViewLX[1],
        ViewLY[1]);

    // Convert to doubles
    ViewX[0] = (double)f_longitude0;
    ViewY[0] = (double)f_latitude0;
    ViewX[1] = (double)f_longitude1;
    ViewY[1] = (double)f_latitude1;
    ViewZ[0] = 0.0;
    ViewZ[1] = 0.0;


    sdts_elevation_in_meters = 0;

    // If it is an SDTS file, determine whether we may have
    // elevation in feet or meters.  AHPT layer present = feet, AHPR
    // layer present = meters.
    if (strcasecmp(driver_type,"SDTS") == 0) {

        if (OGR_DS_GetLayerByName(datasourceH, "AHPR")) {
            fprintf(stderr,"Elevation is in meters\n");
            sdts_elevation_in_meters = 1;
        }

        if (OGR_DS_GetLayerByName(datasourceH, "AHPT")) {
            fprintf(stderr,"Elevation is in feet\n");
        }
    }


    // If we put these flags down in the layer loop, we end up with
    // all black lines being drawn.  Perhaps the layer that defines
    // the types isn't the same layer that contains the vectors that
    // get drawn?
    //
    hypsography_layer = 0;
    hydrography_layer = 0;
    water_layer = 0;
    roads_trails_layer = 0;
    railroad_layer = 0;
    misc_transportation_layer = 0;



    numLayers = OGR_DS_GetLayerCount(datasourceH);










#ifdef TIGER_POLYGONS
    // Special handling for TIGER files so that we can extract/draw
    // polygons.
    //
    // Tiger Polygons:
    // *) Read "Polygon".  Snag "POLYID" and "CENID" fields.
//DONE!
    // *) Read "PIP".  Match "POLYID" (and "CENID"?) field.  Snag
    //    "POLYLONG", "POLYLAT", and "WATER" fields.  We'll use the
    //    lat/long as the point to draw labels.  "WATER"=1:
    //    Perennial.  "WATER"=2: Intermittent.
//DONE!
    // *) Read "AreaLandmarks".  Match on "POLYID" (and "CENID"?) if
    //    possible.  Snag "LAND" field.
//DONE!
    // *) Read "Landmarks".  Match on "LAND" field.  Snag "CFCC" and 
    //    "LANAME" fields.
//DONE!
    // *) Read "PolyChainLink".  Match "POLYID" to either "POLYIDL" or 
    //    "POLYIDR" (and "CENID" to either "CENIDL" or "CENIDR"?).
    //    Snag "TLID" fields.
//DONE!
    // *) Read the "CompleteChain" layer, caching the geometry of any 
    //    lines that match "TLID" fields found above.  We might just
    //    cache these with the polygon info while drawing the
    //    vectors/points, then if a "polygon_flag" is set, proceed
    //    to the last phases of drawing polygons.  This will allow
    //    us to read in the vector information once instead of
    //    twice.  Actually, reversing this would be better so that
    //    we draw vectors on top of the polygons, so that roads and
    //    railroads and such will get drawn on top of water, streams
    //    will get drawn on top of city color-fill, etc.
//DONE!
    // *) Once we've processed the CompleteChain layer, turn those 
    //    geometries into real polygons:
    //      Create a wkbGeometryCollection object.
//DONE!
    //      BuildPolygonFromEdges()
//DONE!
    //      Draw the polygons.  Decide whether to draw based on the
    //        WATER field and/or the CFCC field if present.
//DONE!
    // *) Draw non-polygon vectors.
//DONE!
    //
    if (draw_filled && strcasecmp(driver_type,"TIGER") == 0) {
#define POLYID_HASH_SIZE  64000
#define TLID_HASH_SIZE   240000
#define LAND_HASH_SIZE     5000
        struct hashtable *polyid_hash;
        struct hashtable *landmark_hash;
        struct hashtable *tlid_hash;

        OGRLayerH layerH;
        OGRFeatureH featureH;
        struct hashtable_itr *iterator = NULL;

        typedef struct _tlid_struct {
            int TLID;  // Max value is 2^31 or 2,147,483,647
            struct _tlid_struct *next;
        }tlid_struct;

        typedef struct _polyinfo {
            int POLYID;      // From Polygon, polygon ID
            char CENID[6];   // From Polygon, many others
            double POLYLONG; // From PIP
            double POLYLAT;  // From PIP
            int WATER;       // From PIP, 1 or 2 = water
            int LAND;        // From AreaLandmarks, Landmark ID
            char CFCC[4];    // From Landmarks
            char LANAME[31]; // From Landmarks, Landmark name
            struct _tlid_struct *tlid_list; // From PolyChainLink
        }polyinfo;

        typedef struct _tlidinfo {
            int TLID;      // Tiger/Line ID
            char FEDIRP[3]; // Size from Tiger 2003 definitions
            char FENAME[31];// Size from Tiger 2003 definitions
            char FETYPE[5]; // Size from Tiger 2003 definitions
            char FEDIRS[3]; // Size from Tiger 2003 definitions
            OGRGeometryH geometryH;
        }tlidinfo;

        // For AreaLandmarks and Landmarks data.  Allows us to look
        // up the POLYID integer given the LAND integer.
        typedef struct _landmarkinfo {
            int LAND;        // From AreaLandmarks
            int POLYID;      // From AreaLandmarks
            char CENID[6];   // From AreaLandmarks
        }landmarkinfo;

// FUNCTION:
        // Create hash from key.  LAND is unique within each county.
        // We perform the modulus function on it with the size of
        // our hash to derive the hash key.
        //
        unsigned int landmark_hash_from_key(void *key) {
            landmarkinfo *temp = key;

            return(temp->LAND % LAND_HASH_SIZE);
        }

// FUNCTION:
        // Test equality of hash keys.  In this case LAND is the key
        // we care about.
        //
        int landmark_keys_equal(void *key1, void *key2) {
            landmarkinfo *temp1 = key1;
            landmarkinfo *temp2 = key2;

            if (temp1->LAND == temp2->LAND)
                return(1);  // LAND fields match
            else
                return(0);  // No match
        }

// FUNCTION:
        // Create hash from key.  POLYID is unique within each
        // CENID.  We perform the modulus function on it with the
        // size of our hash to derive the hash key.
        //
        unsigned int polyid_hash_from_key(void *key) {
            polyinfo *temp = key;

            return(temp->POLYID % POLYID_HASH_SIZE);
        }

// FUNCTION:
        // Test equality of hash keys.  In this case POLYID (and
        // CENID?) are the keys we care about.
        //
        int polyid_keys_equal(void *key1, void *key2) {
            polyinfo *temp1 = key1;
            polyinfo *temp2 = key2;

            if (temp1->POLYID == temp2->POLYID)
                return(1);  // POLYID fields match
            else
                return(0);  // No match
        }

// FUNCTION:
        // Create hash from key.  TLID is unique across the U.S.  We
        // perform the modulus function on it with the size of our
        // hash to derive the hash key.
        //
        unsigned int tlid_hash_from_key(void *key) {
            tlidinfo *temp = key;

            return(temp->TLID % TLID_HASH_SIZE);
        }

// FUNCTION:
        // Test equality of hash keys.  In this case TLID is the
        // absolute key that we care about.
        //
        int tlid_keys_equal(void *key1, void *key2) {
            tlidinfo *temp1 = key1;
            tlidinfo *temp2 = key2;

            if (temp1->TLID == temp2->TLID)
                return(1);  // TLID fields match
            else
                return(0);  // No match
        }



        // Create the empty hash table for polyid
        polyid_hash = create_hashtable(POLYID_HASH_SIZE,
            polyid_hash_from_key,
            polyid_keys_equal);

        // Create the empty hash table for "landmark"
        landmark_hash = create_hashtable(LAND_HASH_SIZE,
            landmark_hash_from_key,
            landmark_keys_equal);

        // Create the empty hash table for tlid
        tlid_hash = create_hashtable(TLID_HASH_SIZE,
            tlid_hash_from_key,
            tlid_keys_equal);


fprintf(stderr,"Starting polygon reassembly\n");
fprintf(stderr,"                      Polygon Layer ");
start_timer();


        layerH = OGR_DS_GetLayerByName(datasourceH, "Polygon");
        if (layerH) {
            // Loop through all of the "Polygon" layer records,
            // saving the POLYID/CENID fields.
            polyinfo *temp;

            while ( (featureH = OGR_L_GetNextFeature( layerH )) != NULL ) {
                int kk;

                kk = OGR_F_GetFieldIndex( featureH, "POLYID");
                if (kk != -1) {
                    int polyid;
                    int mm;

                    polyid = OGR_F_GetFieldAsInteger( featureH, kk);

                    // Allocate struct
                    temp = (polyinfo *)malloc(sizeof(polyinfo));
                    temp->POLYID = polyid;

//fprintf(stderr,"POLYID:%i\n", polyid);

                    // Fill in some defaults
                    temp->WATER = 0;
                    temp->LAND = -1;
                    temp->CFCC[0] = '\0';
                    temp->LANAME[0] = '\0';
                    temp->tlid_list = NULL;

                    mm = OGR_F_GetFieldIndex( featureH, "CENID");
                    if (mm != -1) {
                        const char *cenid;

                        cenid = OGR_F_GetFieldAsString( featureH, mm);
                        xastir_snprintf(temp->CENID,
                            sizeof(temp->CENID),
                            "%s",
                            cenid);
//fprintf(stderr,"CENID:%s\n", cenid);
                    }
                    else {
                        temp->CENID[0] = '\0';
                    }

                    // Insert a new value into the hash
// Remember to free() the hash storage later
 
                    if (!hashtable_insert(polyid_hash,&temp->POLYID, temp)) {
                        fprintf(stderr,"Couldn't insert into polyid_hash\n");
                    }
//fprintf(stderr,"%i ",temp->POLYID);
                }

                if (featureH != NULL)
                    OGR_F_Destroy( featureH );

            }   // End of while
        }   // End of if (layerH)


stop_timer(); print_timer_results(); start_timer();
fprintf(stderr,"                          PIP Layer ");


        layerH = OGR_DS_GetLayerByName(datasourceH, "PIP");
        if (layerH) {
            // Loop through all of the "PIP" layer records,
            // saving the POLYLONG/POLYLAT/WATER fields.

            while ( (featureH = OGR_L_GetNextFeature( layerH )) != NULL ) {
                int kk;

                kk = OGR_F_GetFieldIndex( featureH, "POLYID");
                if (kk != -1) {
                    int polyid;
                    polyinfo *found;


                    polyid = OGR_F_GetFieldAsInteger( featureH, kk);

                    // Find an entry in the hash
                    found = hashtable_search(polyid_hash, &polyid);
                    if (found) {    // Found a match!
                        int mm;

//fprintf(stderr,"Found a match for POLYID in PIP layer!\n");

                        mm = OGR_F_GetFieldIndex( featureH, "POLYLONG");
                        if (mm != -1) {
                                double polylong;

//fprintf(stderr,"POLYLONG\t");
                                polylong = OGR_F_GetFieldAsDouble( featureH, mm);
                                found->POLYLONG = polylong;
                        }

                        mm = OGR_F_GetFieldIndex( featureH, "POLYLAT");
                        if (mm != -1) {
                                double polylat;
 
//fprintf(stderr,"POLYLAT\t");
                                polylat = OGR_F_GetFieldAsDouble( featureH, mm);
                                found->POLYLAT = polylat;
                        }

                        mm = OGR_F_GetFieldIndex( featureH, "WATER");
                        if (mm != -1) {
                                int water;
 
//fprintf(stderr,"WATER\n");
                                water = OGR_F_GetFieldAsInteger( featureH, mm);
                                found->WATER = water;
                        }
                    }
                }

                if (featureH != NULL)
                    OGR_F_Destroy( featureH );

            }   // End of while
        }   // End of if (layerH)


stop_timer(); print_timer_results(); start_timer();
fprintf(stderr,"                AreaLandmarks Layer ");


        layerH = OGR_DS_GetLayerByName(datasourceH, "AreaLandmarks");
        if (layerH) {
            // Loop through all of the "AreaLandmarks" layer records,
            // saving the LAND fields in landmark_hash.

            while ( (featureH = OGR_L_GetNextFeature( layerH )) != NULL ) {
                int kk;

                kk = OGR_F_GetFieldIndex( featureH, "LAND");
                if (kk != -1) {
                    int land;

                    land = OGR_F_GetFieldAsInteger( featureH, kk);

                    kk = OGR_F_GetFieldIndex( featureH, "POLYID");
                    if (kk != -1) {
                        int polyid;
                        landmarkinfo *temp;

                        polyid = OGR_F_GetFieldAsInteger( featureH, kk);
 
                        // Allocate struct
                        temp = (landmarkinfo *)malloc(sizeof(landmarkinfo));
                        temp->LAND = land;
                        temp->POLYID = polyid;
 
                        kk = OGR_F_GetFieldIndex( featureH, "CENID");
                        if (kk != -1) {
                            const char *cenid;

                            cenid = OGR_F_GetFieldAsString( featureH, kk);
                            xastir_snprintf(temp->CENID,
                                sizeof(temp->CENID),
                                "%s",
                                cenid);
                        }
                        else {
                            temp->CENID[0] = '\0';
                        }

                        // Insert a new value into the hash
// Remember to free() the hash storage later
                        if (!hashtable_insert(landmark_hash,&temp->LAND, temp)) {
                            fprintf(stderr,"Couldn't insert into landmark_hash\n");
                        }
                    }
                }

                if (featureH != NULL)
                    OGR_F_Destroy( featureH );

            }   // End of while
        }   // End of if (layerH)


stop_timer(); print_timer_results(); start_timer();
fprintf(stderr,"                    Landmarks Layer ");


        layerH = OGR_DS_GetLayerByName(datasourceH, "Landmarks");
        if (layerH) {
            // Loop through all of the "Landmarks" layer records,
            // use the LAND value to search for the POLYID value in
            // the landmark hash, then use the POLYID value to find
            // the actual record in the polyid_hash and fill in the
            // LANAME/CFCC fields there.
            //
            while ( (featureH = OGR_L_GetNextFeature( layerH )) != NULL ) {
                int kk;

                kk = OGR_F_GetFieldIndex( featureH, "LAND");
                if (kk != -1) {
                    int land;
                    landmarkinfo *found;

                    land = OGR_F_GetFieldAsInteger( featureH, kk);

                    // Find an entry in landmark_hash
                    found = hashtable_search(landmark_hash, &land);
                    if (found) {    // Found a match!
                        polyinfo *found2;

//fprintf(stderr,"Found a match for LAND (%i) in Landmarks layer!\n", land);

                        // Find the corresponding entry in the polyid_hash
                        found2 = hashtable_search(polyid_hash, &found->POLYID);
                        if (found2) {   // We have our destination record
                            int mm;

//fprintf(stderr,"Found a match for POLYID (%i) in Landmarks layer!\n", found2->POLYID);

                            mm = OGR_F_GetFieldIndex( featureH, "CFCC");
                            if (mm != -1) {
                                const char *cfcc;

                                cfcc = OGR_F_GetFieldAsString( featureH, mm);
//fprintf(stderr,"CFCC:%s\n", cfcc);
                                xastir_snprintf(found2->CFCC,
                                    sizeof(found2->CFCC),
                                    "%s",
                                    cfcc);
                            }

                            mm = OGR_F_GetFieldIndex( featureH, "LANAME");
                            if (mm != -1) {
                                const char *laname;

                                laname = OGR_F_GetFieldAsString( featureH, mm);
//fprintf(stderr,"LANAME:%s\n",laname);
 
                                xastir_snprintf(found2->LANAME,
                                    sizeof(found2->LANAME),
                                    "%s",
                                    laname);
                            }
                        }
                    }
                }

                if (featureH != NULL)
                    OGR_F_Destroy( featureH );

            }   // End of while

            // Restart reads of this layer at the first feature.
            OGR_L_ResetReading(layerH);
        }



// The below section is probably not correct yet.  I'll assume that
// we need to watch how we do the right/left thing and construct our
// chain appropriately, else we'll end up with weird polygons that
// have all of the correct vertices but the wrong line segments
// between them.
// We also need to collect the geometry information for all of the
// matches and link them together, so we can then use
// OGRBuildPolygonFromEdges() to construct a real polygon that we
// can draw.
//


stop_timer(); print_timer_results(); start_timer();
fprintf(stderr,"                PolyChainLink Layer ");

 
        layerH = OGR_DS_GetLayerByName(datasourceH, "PolyChainLink");
        if (layerH) {
            // Match "POLYID" from above to either "POLYIDL" or
            // "POLYIDR" fields.  Save the "TLID" fields on matches.
            while ( (featureH = OGR_L_GetNextFeature( layerH )) != NULL ) {
                int kk, mm, nn;
                int polyidl = -1;
                int polyidr = -1;
                int tlid = -1;
                polyinfo *found1;
                polyinfo *found2;


                kk = OGR_F_GetFieldIndex( featureH, "POLYIDL" );
                mm = OGR_F_GetFieldIndex( featureH, "POLYIDR" );
                nn = OGR_F_GetFieldIndex( featureH, "TLID" );

                if (kk != -1)
                    polyidl = OGR_F_GetFieldAsInteger( featureH, kk);
                if (mm != -1)
                    polyidr = OGR_F_GetFieldAsInteger( featureH, mm);
                if (nn != -1)
                    tlid = OGR_F_GetFieldAsInteger( featureH, nn);


                // Find an entry in the hash.  Check both polyidl
                // and polyidr for matches this time.  If either
                // match, add to the record.
                //
                found1 = hashtable_search(polyid_hash, &polyidl);
                found2 = hashtable_search(polyid_hash, &polyidr);

                if (found1 || found2) { // Found at least one match!
                    tlid_struct *tlid_temp;

//fprintf(stderr,"Found a match for POLYIDL or POLYIDR in PolyChainLink layer!\n");

                    // Link it in at the END of the list to keep the
                    // line order correct.  These are short lists on
                    // average so we're not losing too much time by
                    // traversing down the list each time.  We could
                    // later make it a doubly-linked list in order
                    // to speed things up.
                    //
                    if (found1) {
                        // Allocate a new record and link it in at the
                        // end of the list.  Fill it in with the value
                        // of TLID.
                        tlid_temp = (tlid_struct *)malloc(sizeof(tlid_struct));
                        tlid_temp->TLID = tlid;
                        tlid_temp->next = NULL;

/*
                        if (found1->tlid_list == NULL) {
                            // List is NULL:  Add the new record
                            found1->tlid_list = tlid_temp;
                        }
                        else {  // List has at least one record.
                                // Traverse to the end of the list.
                            tlid_struct *p;

                            p = found1->tlid_list;  // Head of list
                            while (p->next != NULL) {
                                p = p->next;
                            }
                            // We should be sitting at the last record.
                            // Add our new record to the end.
                            p->next = tlid_temp;
                        }
*/

// Try linking it at the beginning of the list instead, which will
// change the drawing order of the vertices but should be much
// faster.
tlid_temp->next = found1->tlid_list;
found1->tlid_list = tlid_temp;

                    }
                    if (found2) {
                        // Allocate a new record and link it in at the
                        // end of the list.  Fill it in with the value
                        // of TLID.
                        tlid_temp = (tlid_struct *)malloc(sizeof(tlid_struct));
                        tlid_temp->TLID = tlid;
                        tlid_temp->next = NULL;

/*
                        if (found2->tlid_list == NULL) {
                            // List is NULL:  Add the new record
                            found2->tlid_list = tlid_temp;
                        }
                        else {  // List has at least one record.
                                // Traverse to the end of the list.
                            tlid_struct *p;

                            p = found2->tlid_list;  // Head of list
                            while (p->next != NULL) {
                                p = p->next;
                            }
                            // We should be sitting at the last record.
                            // Add our new record to the end.
                            p->next = tlid_temp;
                        }
*/

// Try linking it at the beginning of the list instead, which will
// change the drawing order of the vertices but should be much
// faster.
tlid_temp->next = found2->tlid_list;
found2->tlid_list = tlid_temp;

                    }
                }

                if (featureH != NULL)
                    OGR_F_Destroy( featureH );

            }   // End of while
        }


stop_timer(); print_timer_results(); start_timer();
fprintf(stderr,"                CompleteChain Layer ");


        // Run through the CompleteChain layer.  Snag TLID, FEDIRP,
        // FENAME, FETYPE, FEDIRS, and geometry, store it in a new
        // hash by TLID.
        //
        layerH = OGR_DS_GetLayerByName(datasourceH, "CompleteChain");
        if (layerH) {
            tlidinfo *temp;

            // Create a new hash table with TLID as the hash key.
            while ( (featureH = OGR_L_GetNextFeature( layerH )) != NULL ) {
                int kk;

                kk = OGR_F_GetFieldIndex( featureH, "TLID");
                if (kk != -1) {
                    int ll,mm,nn,oo;
                    int tlid;
                    const char *fedirp;
                    const char *fename;
                    const char *fetype;
                    const char *fedirs;
                    OGRGeometryH geometryH;
 

                    tlid = OGR_F_GetFieldAsInteger(featureH, kk);
 
                    // Allocate struct
                    temp = (tlidinfo *)malloc(sizeof(tlidinfo));
                    temp->TLID = tlid;
                    temp->FEDIRP[0] = '\0';
                    temp->FENAME[0] = '\0';
                    temp->FETYPE[0] = '\0';
                    temp->FEDIRS[0] = '\0';
                    temp->geometryH = NULL;

//fprintf(stderr,"TLID:%i\n", tlid);

                    ll = OGR_F_GetFieldIndex( featureH, "FEDIRP");
                    mm = OGR_F_GetFieldIndex( featureH, "FENAME");
                    nn = OGR_F_GetFieldIndex( featureH, "FETYPE");
                    oo = OGR_F_GetFieldIndex( featureH, "FEDIRS");
 
                    if (ll != -1) {
                        fedirp = OGR_F_GetFieldAsString( featureH, ll);
                        if (fedirp != NULL) {
                            xastir_snprintf(temp->FEDIRP,
                                sizeof(temp->FEDIRP),
                                "%s",
                                fedirp);
                        }
                    }
                    if (mm != -1) {
                        fename = OGR_F_GetFieldAsString( featureH, mm);
                        if (fename != NULL) {
                            xastir_snprintf(temp->FENAME,
                                sizeof(temp->FENAME),
                                "%s",
                                fename);
                        }
                    }
                    if (nn != -1) {
                        fetype = OGR_F_GetFieldAsString( featureH, nn);
                        if (fetype != NULL) {
                            xastir_snprintf(temp->FETYPE,
                                sizeof(temp->FETYPE),
                                "%s",
                                fetype);
                        }
                    }
                    if (oo != -1) {
                        fedirs = OGR_F_GetFieldAsString( featureH, oo);
                        if (fedirs != NULL) {
                            xastir_snprintf(temp->FEDIRS,
                                sizeof(temp->FEDIRS),
                                "%s",
                                fedirs);
                        }
                    }


                    // Get a handle to the geometry itself, make a
                    // clone of it and tie it to the struct.
                    geometryH = OGR_F_GetGeometryRef(featureH);
                    if (geometryH) {
                        temp->geometryH = OGR_G_Clone(geometryH);
                    }


                    // Insert a new value into the hash
// Remember to free() the hash storage later
                    if (!hashtable_insert(tlid_hash,&temp->TLID, temp)) {
                        fprintf(stderr,"Couldn't insert into tlid_hash\n");
                    }
                }

                if (featureH != NULL)
                    OGR_F_Destroy( featureH );

            }   // End of while

            // Restart reads of this layer at the first feature.
            OGR_L_ResetReading(layerH);

        }   // End of CompleteChain layer section


stop_timer(); print_timer_results(); start_timer();
fprintf(stderr,"Combine Hashes, Create/Draw Polygon ");


        // We have two hashes with part of the info in each of them.
        // We should be able to iterate through the polyid_hash,
        // snag the TLID parameters out of each polygon, and then
        // snag the geometry for each TLID out of the tlid_hash.
        // Through this method we can draw each polygon one at a
        // time by creating a polygon geometry out of each one.

        // Iterate over the polyid_hash, drawing polygons as we go.
        //
        if (hashtable_count(polyid_hash) > 0) {
//fprintf(stderr,"polyid_hash\n");
            iterator = hashtable_iterator(polyid_hash);
//fprintf(stderr,"got iterator\n");
            do {
                polyinfo *record;
                tlid_struct *head;
                OGRGeometryH collectionH;
                OGRGeometryH newpolygonH;
                OGRErr error;


                record = hashtable_iterator_value(iterator);

//fprintf(stderr,"got record\n");


// Create an empty geometry collection that we can add other
// geometries to.
                collectionH = OGR_G_CreateGeometry(wkbGeometryCollection);

                if (collectionH == NULL)
                    fprintf(stderr,"collectionH is empty\n");


                // Iterate through the tlid_list linked list.  Look
                // up each TLID in the tlid_hash.
                head = record->tlid_list;
                while (head) {
                    tlidinfo *found;
                    OGRErr error;

                    // Find this TLID in the tlid_hash
                    found = hashtable_search(tlid_hash, &head->TLID);
                    if (found) {    // Found a match!
//fprintf(stderr,"T");
                        // Snag the geometry associated with this
                        // TLID.  Add it to the geometry collection
                        // for this POLYID.
                        error = OGR_G_AddGeometry(collectionH,
                            found->geometryH);
                        if (error != OGRERR_NONE)
                            fprintf(stderr,
                                "OGR_G_AddGeometry: Unsupported geometry type?\n");
                    }
                    else {
//fprintf(stderr,"?");
                    }
 
                    // Skip to the next tlid entry
                    head = head->next;
                }


// Create a polygon geometry out of the geometry collection.
                newpolygonH = OGRBuildPolygonFromEdges(collectionH,
                    1,        // bBestEffort
                    1,        // bAutoClose
                    0.0,   // dfTolerance
                    &error);  // OGRerr
                if (error == OGRERR_NONE) {
//fprintf(stderr,"\t\t\tCreated polygon!\n");
                }
                else {
                    fprintf(stderr,"Failed to create polygon!\n");
                }


                // Draw the polygons.  All of the information to do
                // so is available now.
/*
                guess_vector_attributes(w,
                    driver_type,
                    full_filename,
                    layerH,
                    featureH,
                    geometry_type);
*/

// For testing purposes only draw the water polygons.

//if (record->WATER > 0 && record->CFCC[0] != 'H')
//    fprintf(stderr,"WATER polygon but no \'H\' CFCC!  CFCC=%s\n", record->CFCC);

                if (record->WATER > 0 || record->CFCC[0] == 'H') {

                    if (strncasecmp(record->CFCC,"H81",3) == 0) { // glacier
                        label_color_guess = 0x4d;   // white
//fprintf(stderr,"CFCC:%s\t", record->CFCC);
//fprintf(stderr,"%s\n", record->LANAME);
                    }
                    else {
                        label_color_guess = 0x1a;   // Steel Blue
                    }
 
                    if (label_color_guess != -1) {
                        Draw_OGR_Polygons(w,
                            //featureH,
                            NULL,
                            newpolygonH,
                            1,
                            transformH,
                            draw_filled,
                            //fast_extents);
                            1);
                    }
                }


                // Free the polygon structure
                OGR_G_DestroyGeometry(newpolygonH);

                // Free the geometry collection structure
                OGR_G_DestroyGeometry(collectionH);

            // Free the current hash element, advance to the next
            } while (hashtable_iterator_advance(iterator));

            free(iterator);
        }


stop_timer(); print_timer_results(); start_timer();
fprintf(stderr,"               Free'ing hash memory ");


        // Free the memory we've allocated for the hashes.


        // Iterate over the polyid_hash, free'ing the tlid_struct's
        // that we allocated.  Don't remove the hash entry or free
        // the polyinfo struct inside the loop as that will break
        // the iterator.
        //
//fprintf(stderr,"Hash elements: polyid:%6i   ", hashtable_count(polyid_hash));
        if (hashtable_count(polyid_hash) > 0) {
            iterator = hashtable_iterator(polyid_hash);
            do {
                polyinfo *record;
                tlid_struct *head;
                tlid_struct *p;

                record = hashtable_iterator_value(iterator);

                // Free the tlid_struct linked list
                head = record->tlid_list;
                while (head) {
//fprintf(stderr,"TLID ");
                    p = head;
                    head = head->next;
                    free(p);
                }

            // Free the current hash element, advance to the next
            } while (hashtable_iterator_remove(iterator));

            free(iterator);
        }
        // polyid_hash should be empty at this point.


        // Iterate over the tlid_hash, free'ing the geometry memory
        // that we allocated.  Don't remove the hash entry or free
        // the tlidinfo struct inside the loop as that will break
        // the iterator.
        //
//fprintf(stderr,"tlid:%6i   ", hashtable_count(tlid_hash));
        if (hashtable_count(tlid_hash) > 0) {
            iterator = hashtable_iterator(tlid_hash);
            do {
                tlidinfo *record;

                record = hashtable_iterator_value(iterator);

                // Free the geometry structure
                OGR_G_DestroyGeometry(record->geometryH);

            // Free the current hash element, advance to the next
            } while (hashtable_iterator_remove(iterator));

            free(iterator);
        }
        // tlid_hash should be empty at this point.


        // Iterate over the landmark_hash, free'ing the memory
        // that we allocated.  Don't remove the hash entry of free
        // the landmarkinfo struct inside the loop as that will
        // break the iterator.
        //
//fprintf(stderr,"landmark:%6i\n", hashtable_count(landmark_hash));
        if (hashtable_count(landmark_hash) > 0) {
            iterator = hashtable_iterator(landmark_hash);
            do {
            // Free the current hash element, advance to the next
            } while (hashtable_iterator_remove(iterator));

            free(iterator);
        }
        // landmark_hash should be empty at this point.


        // Destroy the hash tables.  If the second argument is a
        // one, it indicates that the values should be free'd as
        // well.
        //
        hashtable_destroy(polyid_hash, 1);
        hashtable_destroy(tlid_hash, 1);
        hashtable_destroy(landmark_hash, 1);


stop_timer(); print_timer_results();
//fprintf(stderr,"Done\n");


    }   // End of special TIGER section

#endif  // TIGER_POLYGONS










    // Loop through all layers in the data source.
    //
    // Optimizations:
    //   SDTS, load only those layers that make sense.
    //   TIGER, same thing, based on the type of file.  We probably need
    //     to do some serious changes to how we do TIGER layers anyway,
    //     as we have to pull multiple things together in order to draw
    //     polygons.
    //
    for ( ii=0; ii<numLayers; ii++ ) {
        OGRLayerH layerH;
        OGRFeatureH featureH;
//        OGRFeatureDefnH layerDefn;
        OGREnvelope psExtent;  
        int extents_found = 0;
        char geometry_type_name[50] = "";
        int geometry_type = -1;
        int fast_extents = 0;
        int features_processed = 0;
        const char *layer_name;
        

//fprintf(stderr,"Layer %d:\n", ii); 

        HandlePendingEvents(app_context);
        if (interrupt_drawing_now) {

            if (wgs84_spatialH != NULL) {
                OSRDestroySpatialReference(wgs84_spatialH);
            }

//            if (transformH != NULL) {
//                OCTDestroyCoordinateTransformation(transformH);
//            }

//            if (reverse_transformH != NULL) {
//                OCTDestroyCoordinateTransformation(reverse_transformH);
//            }

            // Close data source
            if (datasourceH != NULL) {
                OGR_DS_Destroy( datasourceH );
            }

            return; // Exit early
        }

        layerH = OGR_DS_GetLayer( datasourceH, ii );

        if (layerH == NULL) {
            if (debug_level & 16) {
                fprintf(stderr,
                    "Unable to open layer %d of %s\n",
                    ii,
                   full_filename);
            }

            if (wgs84_spatialH != NULL) {
                OSRDestroySpatialReference(wgs84_spatialH);
            }

//            if (transformH != NULL) {
//                OCTDestroyCoordinateTransformation(transformH);
//            }

//            if (reverse_transformH != NULL) {
//                OCTDestroyCoordinateTransformation(reverse_transformH);
//            }

            // Close data source
            if (datasourceH != NULL) {
                OGR_DS_Destroy( datasourceH );
            }

            return; // Exit early
        }


        // Determine what kind of layer we're dealing with and set
        // some flags.
        //
        layer_name = OGR_FD_GetName( OGR_L_GetLayerDefn( layerH ) );

        if (layer_name != NULL) {

//fprintf(stderr,"Layer %i: %s\n", ii, layer_name);

            if (strcasecmp(driver_type,"SDTS") == 0) {
                //
                // Layer  0: ARDF Roads/Trails Layer
                // Layer  1: ARRF Railroad Layer
                // Layer  2: AMTF Misc Transportation Layer
                // Layer  3: ARDM
                // Layer  4: ACOI
                // Layer  5: AHDR
                // Layer  6: NP01
                // Layer  7: NP02
                // Layer  8: NP03
                // Layer  9: NP04
                // Layer 10: NP05
                // Layer 11: NP06
                // Layer 12: NP07
                // Layer 13: NP08
                // Layer 14: NP09
                // Layer 15: NP10
                // Layer 16: NP11
                // Layer 17: NP12
                // Layer 18: NA01
                // Layer 19: NA02
                // Layer 20: NA03
                // Layer 21: NA04
                // Layer 22: NA05
                // Layer 23: NA06
                // Layer 24: NA07
                // Layer 25: NA08
                // Layer 26: NA09
                // Layer 27: NA10
                // Layer 28: NA11
                // Layer 29: NA12
                // Layer 30: NO01
                // Layer 31: NO02
                // Layer 32: NO03
                // Layer 33: NO04
                // Layer 34: NO05
                // Layer 35: NO06
                // Layer 36: NO07
                // Layer 37: NO08
                // Layer 38: NO09
                // Layer 39: NO10
                // Layer 40: NO11
                // Layer 41: NO12
                // Layer 42: LE01
                // Layer 43: LE02
                // Layer 44: LE03
                // Layer 45: LE04
                // Layer 46: LE05
                // Layer 47: LE06
                // Layer 48: LE07
                // Layer 49: LE08
                // Layer 50: LE09
                // Layer 51: LE10
                // Layer 52: LE11
                // Layer 53: LE12
                // Layer 54: PC01
                // Layer 55: PC02
                // Layer 56: PC03
                // Layer 57: PC04
                // Layer 58: PC05
                // Layer 59: PC06
                // Layer 60: PC07
                // Layer 61: PC08
                // Layer 62: PC09
                // Layer 63: PC10
                // Layer 64: PC11
                // Layer 65: PC12
                //
                if (strncasecmp(layer_name,"AHPF",4) == 0) {
                    hypsography_layer++;    // Topo contours
                    fprintf(stderr,"Hypsography Layer (topo contours)\n");
                }
                else if (strncasecmp(layer_name,"AHYF",4) == 0) {
                    hydrography_layer++;    // Underwater contours
                    fprintf(stderr,"Hydrography Layer (underwater topo contours)\n");
                }
                else if (strncasecmp(layer_name,"ARDF",4) == 0) {
                    roads_trails_layer++;
                    fprintf(stderr,"Roads/Trails Layer\n");
                }
                else if (strncasecmp(layer_name,"ARRF",4) == 0) {
                    railroad_layer++;
                    fprintf(stderr,"Railroad Layer\n");
                }
                else if (strncasecmp(layer_name,"AMTF",4) == 0) {
                    misc_transportation_layer++;
                    fprintf(stderr,"Misc Transportation Layer\n");
                }
            }
            else if (strcasecmp(driver_type,"TIGER") == 0) {
                //
                // Layer  0: CompleteChain
                // Layer  1: AltName
                // Layer  2: FeatureIds
                // Layer  3: ZipCodes
                // Layer  4: Landmarks
                // Layer  5: AreaLandmarks
                // Layer  6: Polygon
                // Layer  7: PolygonCorrections
                // Layer  8: EntityNames
                // Layer  9: PolygonEconomic
                // Layer 10: IDHistory
                // Layer 11: PolyChainLink
                // Layer 12: PIP
                // Layer 13: TLIDRange
                // Layer 14: ZeroCellID
                // Layer 15: OverUnder
                // Layer 16: ZipPlus4
                //
                if (strncasecmp(layer_name,"AltName",7) == 0) {
                    continue;   // Skip this layer
                }
                else if (strncasecmp(layer_name,"ZipCodes",8) == 0) {
                    continue;   // Skip this layer
                }
                else if (strncasecmp(layer_name,"Landmarks",9) == 0) {
                    continue;   // Skip this layer
                }
                else if (strncasecmp(layer_name,"PolygonCorrections",18) == 0) {
                    continue;   // Skip this layer
                }
                else if (strncasecmp(layer_name,"AreaLandmarks",13) == 0) {
                    continue;   // Skip this layer
                }
                else if (strncasecmp(layer_name,"PolygonEconomic",15) == 0) {
                    continue;   // Skip this layer
                }
                else if (strncasecmp(layer_name,"IDHistory",9) == 0) {
                    continue;   // Skip this layer
                }
                else if (strncasecmp(layer_name,"ZeroCellID",10) == 0) {
                    continue;   // Skip this layer
                }
                else if (strncasecmp(layer_name,"OverUnder",9) == 0) {
                    continue;   // Skip this layer
                }
                else if (strncasecmp(layer_name,"ZipPlus4",8) == 0) {
                    continue;   // Skip this layer
                }
            }
        }


//fprintf(stderr,"    Processing Layer %i\n", ii);



        // Set up the coordinate translations we may need for both
        // indexing and drawing operations.  It sets up the
        // transform and the reverse transforms we need to convert
        // between the spatial coordinate systems.
        //
        setup_coord_translation(datasourceH, // Input
            layerH,                          // Input
            &map_spatialH,                   // Output
            &transformH,                     // Output
            &reverse_transformH,             // Output
            &wgs84_spatialH,                 // Output
            &no_spatial,                     // Output
            &geographic,                     // Output
            &projected,                      // Output
            &local,                          // Output
            datum,                           // Output
            geogcs);                         // Output


        // Snag the original values (again if 2nd or later loop
        // iteration).
        ViewX2[0] = ViewX[0];
        ViewX2[1] = ViewX[1];
        ViewY2[0] = ViewY[0];
        ViewY2[1] = ViewY[1];
        if (reverse_transformH) {
            // Convert our view coordinates from WGS84 to this map
            // layer's coordinates.
            if (!OCTTransform(reverse_transformH, 2, ViewX2, ViewY2, ViewZ2)) {
                fprintf(stderr,
                    "Couldn't convert points from WGS84 to map's spatial reference\n");
                // Use the coordinates anyway (don't exit).  We may be
                // lucky enough to have things work anyway.
            }
            // Get rid of our reverse transform.  We shan't need it
            // again.
            OCTDestroyCoordinateTransformation(reverse_transformH);
            reverse_transformH = NULL;
        }

//fprintf(stderr,"%2.5f %2.5f   %2.5f %2.5f\n",
//    ViewY2[0], ViewX2[0], ViewY2[1], ViewX2[1]);


        // Add these converted points to the spatial_filter_geometry so
        // that we can set our spatial filter in the layer loop below.
        // Snag the spatial reference from the map dataset 'cuz they
        // should match now.
        //
        spatial_filter_geometryH = OGR_G_CreateGeometry(2); // LineString Type

        // Use the map spatial geometry so that we match the map
        OGR_G_AssignSpatialReference(spatial_filter_geometryH, map_spatialH);

        // Add the corners of the viewport
        OGR_G_AddPoint(spatial_filter_geometryH, ViewX2[0], ViewY2[0], ViewZ2[0]);
        OGR_G_AddPoint(spatial_filter_geometryH, ViewX2[0], ViewY2[1], ViewZ2[1]);
        OGR_G_AddPoint(spatial_filter_geometryH, ViewX2[1], ViewY2[1], ViewZ2[0]);
        OGR_G_AddPoint(spatial_filter_geometryH, ViewX2[1], ViewY2[0], ViewZ2[1]);

        // Set spatial filter so that the GetNextFeature() call will
        // only return features that are within our view.  Note that
        // the geometry used here should be in the same spacial
        // reference system as the layer itself, so we need to
        // convert our coordinates into the map coordinates before
        // setting the filter.  We do this coordinate conversion and
        // create the geometry outside the layer loop to save some
        // time, then just set the spatial filter with the same
        // geometry for each iteration.
        //
        // Note that this spatial filter doesn't strictly filter at
        // the borders specified, but it does filter out a lot of
        // the features that are outside our borders.  This speeds
        // up map drawing tremendously!
        //
        OGR_L_SetSpatialFilter( layerH, spatial_filter_geometryH);

 

        // Test the capabilities of the layer to know the best way
        // to access it:
        //
        //   OLCRandomRead: TRUE if the OGR_L_GetFeature() function works
        //   for this layer.
        //   NOTE: Tiger and Shapefile report this as TRUE.
        //
        //   OLCFastSpatialFilter: TRUE if this layer implements spatial
        //   filtering efficiently.
        //
        //   OLCFastFeatureCount: TRUE if this layer can return a feature
        //   count (via OGR_L_GetFeatureCount()) efficiently.  In some cases
        //   this will return TRUE until a spatial filter is installed after
        //   which it will return FALSE.
        //   NOTE: Tiger and Shapefile report this as TRUE.
        //
        //   OLCFastGetExtent: TRUE if this layer can return its data extent
        //   (via OGR_L_GetExtent()) efficiently ... ie. without scanning
        //   all the features. In some cases this will return TRUE until a
        //   spatial filter is installed after which it will return FALSE.
        //   NOTE: Shapefile reports this as TRUE.
        //
        if (ii == 0) {   // First layer
            if (debug_level & 16)
                fprintf(stderr, "  ");
            if (OGR_L_TestCapability(layerH, OLCRandomRead)) {
                if (debug_level & 16)
                    fprintf(stderr, "Random Read, ");
            }
            if (OGR_L_TestCapability(layerH, OLCFastSpatialFilter)) {
                if (debug_level & 16)
                    fprintf(stderr,
                        "Fast Spatial Filter, ");
            }
            if (OGR_L_TestCapability(layerH, OLCFastFeatureCount)) {
                if (debug_level & 16)
                    fprintf(stderr,
                        "Fast Feature Count, ");
            }
            if (OGR_L_TestCapability(layerH, OLCFastGetExtent)) {
                if (debug_level & 16)
                    fprintf(stderr,
                        "Fast Get Extent, ");
                // Save this away and decide whether to
                // request/compute extents based on this.
                fast_extents = 1;
            }
        }


/*
        if (map_spatialH) {
            const char *temp;
            int geographic = 0;
            int projected = 0;


            if (OSRIsGeographic(map_spatialH)) {
                if (ii == 0) {
                    if (debug_level & 16)
                        fprintf(stderr,"  Geographic Coord, ");
                }
                geographic++;
            }
            else if (OSRIsProjected(map_spatialH)) {
                if (ii == 0) {
                    if (debug_level & 16)
                        fprintf(stderr,"  Projected Coord, ");
                }
                projected++;
            }
            else {
                if (ii == 0) {
                    if (debug_level & 16)
                        fprintf(stderr,"  Local Coord, ");
                }
            }

            // PROJCS, GEOGCS, DATUM, SPHEROID, PROJECTION
            //
            temp = OSRGetAttrValue(map_spatialH, "DATUM", 0);
            if (ii == 0) {
                if (debug_level & 16)
                    fprintf(stderr,"DATUM: %s, ", temp);
            }

            if (projected) {
                temp = OSRGetAttrValue(map_spatialH, "PROJCS", 0);
                if (ii == 0) {
                    if (debug_level & 16)
                        fprintf(stderr,"PROJCS: %s, ", temp);
                }
 
                temp = OSRGetAttrValue(map_spatialH, "PROJECTION", 0);
                if (ii == 0) {
                    if (debug_level & 16)
                        fprintf(stderr,"PROJECTION: %s, ", temp);
                }
            }

            temp = OSRGetAttrValue(map_spatialH, "GEOGCS", 0);
            if (ii == 0) {
                if (debug_level & 16)
                    fprintf(stderr,"GEOGCS: %s, ", temp);
            }

            temp = OSRGetAttrValue(map_spatialH, "SPHEROID", 0);
            if (ii == 0) {
                if (debug_level & 16)
                    fprintf(stderr,"SPHEROID: %s, ", temp);
            }

        }
        else {
            if (ii == 0) {
                if (debug_level & 16)
                    fprintf(stderr,"  No Spatial Info, ");
                // Assume geographic/WGS84 unless the coordinates go
                // outside the range of lat/long, in which case,
                // exit.
            }
        }
*/



        // Get the extents for this layer.  OGRERR_FAILURE means
        // that the layer has no spatial info or that it would be
        // an expensive operation to establish the extent.
        //OGRErr OGR_L_GetExtent(OGRLayerH hLayer, OGREnvelope *psExtent, int bForce);
        if (OGR_L_GetExtent(layerH, &psExtent, FALSE) != OGRERR_FAILURE) {
            // We have extents.  Check whether any part of the layer
            // is within our viewport.
            if (ii == 0) {
                if (debug_level & 16)
                    fprintf(stderr, "Extents obtained.");
            }
            extents_found++;
        }
        if (ii == 0) {
            if (debug_level & 16)
                fprintf(stderr, "\n");
        }


/*
        if (extents_found) {
            fprintf(stderr,
                "  MinX: %f, MaxX: %f, MinY: %f, MaxY: %f\n",
                psExtent.MinX,
                psExtent.MaxX,
                psExtent.MinY,
                psExtent.MaxY);
        }
*/


// Dump info about this layer
/*
        layerDefn = OGR_L_GetLayerDefn( layerH );
        if (layerDefn != NULL) {
            int jj;
            int numFields;
 
            numFields = OGR_FD_GetFieldCount( layerDefn );

            fprintf(stderr,"  Layer %d: '%s'\n", ii, OGR_FD_GetName(layerDefn));

            for ( jj=0; jj<numFields; jj++ ) {
                OGRFieldDefnH fieldDefn;

                fieldDefn = OGR_FD_GetFieldDefn( layerDefn, jj );
                fprintf(stderr,"    Field %d: %s (%s)\n", 
                       jj, OGR_Fld_GetNameRef(fieldDefn), 
                       OGR_GetFieldTypeName(OGR_Fld_GetType(fieldDefn)));
            }
            fprintf(stderr,"\n");
        }


        // Restart reads of this layer at the first feature.
        //OGR_L_ResetReading(layerH);
*/



// Optimization:  Get the envelope for each feature, skip the
// feature if it's completely outside our viewport.

        // Loop through all of the features in the layer.
        //
//        if ( (featureH = OGR_L_GetNextFeature( layerH ) ) != NULL ) {
//if (0) {
        while ( (featureH = OGR_L_GetNextFeature( layerH )) != NULL ) {
            OGRGeometryH geometryH;
            int num = 0;
//            char *buffer;


features_processed++;
 
            HandlePendingEvents(app_context);
            if (interrupt_drawing_now) {
                if (featureH != NULL)
                    OGR_F_Destroy( featureH );

                if (wgs84_spatialH != NULL) {
                    OSRDestroySpatialReference(wgs84_spatialH);
                }

                if (transformH != NULL) {
                    OCTDestroyCoordinateTransformation(transformH);
                }

//                if (reverse_transformH != NULL) {
//                    OCTDestroyCoordinateTransformation(reverse_transformH);
//                }

                // Close data source
                if (datasourceH != NULL) {
                    OGR_DS_Destroy( datasourceH );
                }

                return; // Exit early
            }

            if (featureH == NULL) {
                continue;
            }


// Debug code
//OGR_F_DumpReadable( featureH, stderr );


            // Get a handle to the geometry itself
            geometryH = OGR_F_GetGeometryRef(featureH);
            if (geometryH == NULL) {
                OGR_F_Destroy( featureH );
//                if (strlen(geometry_type_name) == 0) {
                if (debug_level & 16)
                    fprintf(stderr,"  Layer %02d:   - No geometry info -\n", ii);
//                    geometry_type_name[0] = ' ';
//                    geometry_type_name[1] = '\0';
//                }
// Break out of this loop.  We don't know how to draw anything but
// geometry features yet.  Change this when we start drawing labels.
                break;
//                continue;
            }



            // More debug code.  Print the Well Known Text
            // representation of the geometry.
            //if (OGR_G_ExportToWkt(geometryH, &buffer) == 0) {
            //    fprintf(stderr, "%s\n", buffer);
            //}


            // These are from the OGRwkbGeometryType enumerated set
            // in ogr_core.h:
            //
            //          0 "Unknown"
            //          1 "POINT"              (ogrpoint.cpp)
            //          2 "LINESTRING"         (ogrlinestring.cpp)
            //          3 "POLYGON"            (ogrpolygon.cpp)
            //          4 "MULTIPOINT"         (ogrmultipoint.cpp)
            //          5 "MULTILINESTRING"    (ogrmultilinestring.cpp)
            //          6 "MULTIPOLYGON"       (ogrmultipolygon.cpp)
            //          7 "GEOMETRYCOLLECTION" (ogrgeometrycollection.cpp)
            //        100 "None"
            //        101 "LINEARRING"         (ogrlinearring.cpp)
            // 0x80000001 "Point25D"
            // 0x80000002 "LineString25D"
            // 0x80000003 "Polygon25D"
            // 0x80000004 "MultiPoint25D"
            // 0x80000005 "MultiLineString25D"
            // 0x80000006 "MultiPolygon25D"
            // 0x80000007 "GeometryCollection25D"
            //
            // The geometry type will be the same for any particular
            // layer.  We take advantage of that here by querying
            // once per layer and saving the results in variables.
            //
            if (strlen(geometry_type_name) == 0) {
                xastir_snprintf(geometry_type_name,
                    sizeof(geometry_type_name),
                    "%s",
                    OGR_G_GetGeometryName(geometryH));
                geometry_type = OGR_G_GetGeometryType(geometryH);
//                fprintf(stderr,"  Layer %02d: ", ii); 
//                fprintf(stderr,"  Type: %d, %s\n",  
//                    geometry_type,
//                    geometry_type_name);
            }

// Debug code 
//OGR_G_DumpReadable(geometryH, stderr, "Shape: ");



// We could call OGR_G_GetEnvelope() here and calculate for
// ourselves it if is in our viewport, or we could set a filter and
// let the library pass us only those that fit.
//
// If point or line feature, draw in normal manner.  If polygon
// feature, do the "rotation one way = fill, rotation the other way
// = hole" thing?
//
// We need all of the coordinates in WGS84 lat/long.  Use the
// conversion code that's in the indexing portion of this routine to
// accomplish this.


            switch (geometry_type) {

                case 1:             // Point
                case 4:             // MultiPoint
                case 0x80000001:    // Point25D
                case 0x80000004:    // MultiPoint25D

                    // Do this one time for the file itself to get
                    // some usable defaults.
                    guess_vector_attributes(w,
                        driver_type,
                        full_filename,
                        layerH,
                        featureH,
                        geometry_type);

                    if (label_color_guess != -1) {
                        Draw_OGR_Points(w,
                            featureH,
                            geometryH,
                            1,
                            transformH);
                    }
                    break;

                case 2:             // LineString (polyline)
                case 5:             // MultiLineString
                case 0x80000002:    // LineString25D
                case 0x80000005:    // MultiLineString25D

                    // Do this one time for the file itself to get
                    // some usable defaults.
                    guess_vector_attributes(w,
                        driver_type,
                        full_filename,
                        layerH,
                        featureH,
                        geometry_type);

                    // Special handling for TIGER water polygons,
                    // which are represented as polylines instead of
                    // polygons.
                    //
// How do we get the TIGER water drawn as filled polygons???
                    //
                    if (label_color_guess != -1) {
                        if (water_layer && strstr(driver_type,"TIGER")) {
//fprintf(stderr,"TIGER water layer\n");

                            Draw_OGR_Lines(w,
                                featureH,
                                geometryH,
                                1,
                                transformH,
                                fast_extents);
                        }
                        else {  // Normal processing
                            Draw_OGR_Lines(w,
                                featureH,
                                geometryH,
                                1,
                                transformH,
                                fast_extents);
                        }
                    }
                    break;

                case 3:             // Polygon
                case 6:             // MultiPolygon
                case 0x80000003:    // Polygon25D
                case 0x80000006:    // MultiPolygon25D

                    // Do this one time for the file itself to get
                    // some usable defaults.
                    guess_vector_attributes(w,
                        driver_type,
                        full_filename,
                        layerH,
                        featureH,
                        geometry_type);

                    if (label_color_guess != -1) {
                        Draw_OGR_Polygons(w,
                            featureH,
                            geometryH,
                            1,
                            transformH,
                            draw_filled,
                            fast_extents);
                    }
                    break;

                case 7:             // GeometryCollection
                case 0x80000007:    // GeometryCollection25D
                default:            // Unknown/Unimplemented

                    num = 0;
                    if (debug_level & 16)
                        fprintf(stderr,"  Unknown or unimplemented geometry\n");
                    break;
            }
            if (featureH)
                OGR_F_Destroy( featureH );
        }   // End of feature loop
        // No need to free layerH handle, it belongs to the datasource

//fprintf(stderr,"Features Processed: %d\n\n", features_processed);
    } // End of layer loop

    if (transformH != NULL) {
        OCTDestroyCoordinateTransformation(transformH);
    }

//    if (reverse_transformH != NULL) {
//        OCTDestroyCoordinateTransformation(reverse_transformH);
//    }

    if (wgs84_spatialH != NULL) {
        OSRDestroySpatialReference(wgs84_spatialH);
    }

    // Close data source
    if (datasourceH != NULL) {
        OGR_DS_Destroy( datasourceH );
    }
}



#endif // HAVE_LIBGDAL


