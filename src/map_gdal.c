/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2003  The Xastir Group
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
#include "config.h"
#include "snprintf.h"

//#include <stdio.h>
//#include <stdlib.h>
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

#include "xastir.h"
#include "maps.h"
#include "alert.h"
//#include "util.h"
#include "main.h"
//#include "datum.h"
//#include "draw_symbols.h"
//#include "rotated.h"
//#include "color.h"
//#include "xa_config.h"


#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }



#ifdef HAVE_LIBGDAL

#warning
#warning
#warning GDAL/OGR library support is not fully implemented yet!
#warning Only very preliminary TIGER/Line support has been coded to date.
#warning
#warning

// WE7U - Getting rid of stupid compiler warnings in GDAL
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





void map_gdal_init() {

#ifdef HAVE_LIBGDAL

    GDALDriverH  gDriver;   // GDAL driver
    OGRSFDriverH oDriver;   // OGR driver
    int ii, jj;


    // Register all known GDAL drivers
    GDALAllRegister();

    // Register all known OGR drivers
    OGRRegisterAll();

    // Print out each supported GDAL driver.
    //
    ii = GDALGetDriverCount();

    fprintf(stderr,"\nGDAL Registered Drivers: (not implemented yet)\n");
    for (jj = 0; jj < ii; jj++) {
        gDriver = GDALGetDriver(jj);
        fprintf(stderr,"%10s   %s\n",
            GDALGetDriverShortName(gDriver),
            GDALGetDriverLongName(gDriver) );
    }
    fprintf(stderr,"\n");


    // Print out each supported OGR driver
    //
    ii = OGRGetDriverCount();

    fprintf(stderr,"OGR Registered Drivers: (not implemented yet)\n");
    for  (jj = 0; jj < ii; jj++) {
        oDriver = OGRGetDriver(jj);
        fprintf(stderr,"%10s   %s\n",
            "",
            OGR_Dr_GetName(oDriver) );
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
                   int draw_filled) {

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

    if( hDataset == NULL )
    {
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
    
        if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None )
            {
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

            for( i = 0; i < GDALGetColorEntryCount( hColorTable ); i++ )
                {
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
    GDALRasterIO( hBand, GF_Read, 0, 0, nXSize, 1, 
                  pafScanline, nXSize, 1, GDT_Float32, 
                  0, 0 );

    /* The raster is */






    GDALClose(hDataset);
}





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
// Indexing currently works properly for either geographic or
// projected coordinates, and does conversions to geographic/WGS84
// datum before storing the extents in the map index.  It doesn't
// work for local coordinate systems.
//
void draw_ogr_map(Widget w,
                   char *dir,
                   char *filenm,
                   alert_entry *alert,
                   u_char alert_color,
                   int destination_pixmap,
                   int draw_filled) {

    OGRDataSourceH datasource;
    OGRSFDriverH driver = NULL;
    int i, numLayers;
    char full_filename[300];
    const char *ptr;


    if (debug_level & 16)
        fprintf(stderr,"Entered draw_ogr_map function\n");

    xastir_snprintf(full_filename,
        sizeof(full_filename),
        "%s/%s",
        dir,
        filenm);

    if (debug_level & 16)
        fprintf(stderr,"Opening datasource\n");

    // Open data source
    datasource = OGROpen(full_filename,
        0 /* bUpdate */,
        &driver);

    if (datasource == NULL)
    {
        fprintf(stderr,
            "Unable to open %s\n",
            full_filename);
        return;
    }

    if (debug_level & 16)
        fprintf(stderr,"Opened datasource\n");

    // Implement the indexing functions, so that we can use these
    // map formats from within Xastir.  Without an index, it'll
    // never appear in the map chooser.  Use the OGR "envelope"
    // functions to get the extents for the each layer in turn.
    // We'll find the min/max of all and use that for the extents
    // for the entire dataset.
    //
    // Check whether we're indexing or drawing the map
    if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
            || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {
/////////////////////////////////////////////////////////////////////
        // We're indexing only.  Save the extents in the index.
        char status_text[MAX_FILENAME];
        double file_MinX = 0;
        double file_MaxX = 0;
        double file_MinY = 0;
        double file_MaxY = 0;
        int first_extents = 1;
        int geographic = 0;
        int projected = 0;
        int local = 0;
        const char *geogcs = NULL;
        OGRSpatialReferenceH spatial = NULL;



        xastir_snprintf(status_text,
            sizeof(status_text),
            langcode ("BBARSTA039"),
            filenm);
        statusline(status_text,0);       // Indexing ...

fprintf(stderr,"Indexing %s\n", filenm);

        // Use the OGR "envelope" function to get the extents for
        // the entire file or dataset.  Remember that it could be in
        // state-plane coordinate system or something else equally
        // strange.  Convert it to WGS84 or NAD83 geographic
        // coordinates before saving the index.

        // Loop through all layers in the data source.  We can't get
        // the extents for the entire data source without looping
        // through the layers.
        //
        numLayers = OGR_DS_GetLayerCount(datasource);
        for(i=0; i<numLayers; i++) {
            OGRLayerH layer;
            OGREnvelope psExtent;  


            layer = OGR_DS_GetLayer( datasource, i );
            if (layer == NULL)
            {
                fprintf(stderr,
                    "Unable to open layer %d of %s\n",
                    i,
                    full_filename);

                // Close data source
                OGR_DS_Destroy( datasource );
                return;
            }

            // Query the coordinate system.  Need to have the
            // extents in WGS84 lat/long coordinate system in order
            // to compute the extents properly.
            //
            spatial = OGR_L_GetSpatialRef(layer);
            if (spatial) {
                const char *temp;

                if (OSRIsGeographic(spatial)) {
                    geographic++;
                }
                else if (OSRIsProjected(spatial)) {
                    projected++;
                }
                else {
                    local++;
                }

                // PROJCS, GEOGCS, DATUM, SPHEROID, PROJECTION
                //
                temp = OSRGetAttrValue(spatial, "DATUM", 0);
                if (projected) {
                    temp = OSRGetAttrValue(spatial, "PROJCS", 0);
                    temp = OSRGetAttrValue(spatial, "PROJECTION", 0);
                }
                temp = OSRGetAttrValue(spatial, "SPHEROID", 0);
                geogcs = OSRGetAttrValue(spatial, "GEOGCS", 0);
            }
            else {
fprintf(stderr,"Couldn't get spatial reference\n");
            }

            // Get the extents for this layer.  OGRERR_FAILURE means
            // that the layer has no spatial info or that it would be
            // an expensive operation to establish the extent.
            // Here we set the force option to TRUE in order to read
            // all of the extents even for files where that would be
            // an expensive operation.  We're trying to index the
            // file after all!
            if (OGR_L_GetExtent(layer, &psExtent, TRUE) != OGRERR_FAILURE) {
/*
                fprintf(stderr,
                    "  MinX: %f, MaxX: %f, MinY: %f, MaxY: %f\n",
                    psExtent.MinX,
                    psExtent.MaxX,
                    psExtent.MinY,
                    psExtent.MaxY);
*/

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
            // No need to free layer handle, it belongs to the
            // datasource.
        }

        // We know how to handle geographic or projected coordinate
        // systems.  Test for these.
        if ( !first_extents && (geographic || projected) ) {
            // Need to also check datum!  Must be NAD83 or WGS84 and
            // geographic for our purposes.
            if ( geographic
                    && ( strcasecmp(geogcs,"WGS84") == 0
                        || strcasecmp(geogcs,"NAD83") == 0) ) {

fprintf(stderr, "Geographic coordinate system, %s, adding to index\n", geogcs);

// Debug:  Don't add them to the index so that we can experiment
// with datum translation and such.
//#define WE7U
#ifndef WE7U
                index_update_ll(filenm,    // Filename only
                    file_MinY,  // Bottom
                    file_MaxY,  // Top
                    file_MinX,  // Left
                    file_MaxX); // Right
#endif  // WE7U
            }
            else {  // We have coordinates, but they're either in
                    // the wrong datum or in a projected coordinate
                    // system.  Convert to WGS84.
                OGRSpatialReferenceH wgs84_spatial = NULL;
                OGRCoordinateTransformationH transformH = NULL;

                if (geographic)
fprintf(stderr, "Found geographic/wrong datum: %s.  Converting to wgs84 datum\n", geogcs);
                else
fprintf(stderr, "Found projected coordinates: %s.  Converting to geographic/wgs84 datum\n", geogcs);
 

                wgs84_spatial = OSRNewSpatialReference(NULL);
                if (wgs84_spatial == NULL) {
fprintf(stderr,"Couldn't create empty wgs84_spatial object\n");
                }

                if (OSRSetWellKnownGeogCS(wgs84_spatial,"WGS84") == OGRERR_FAILURE) {
 
                    // Couldn't fill in WGS84 parameters.
fprintf(stderr,"Couldn't fill in wgs84 spatial reference parameters\n");
                    if (wgs84_spatial != NULL)
                        OSRDestroySpatialReference(wgs84_spatial);
                }

                if (spatial == NULL || wgs84_spatial == NULL) {
fprintf(stderr,"Couldn't transform because spatial or wgs84_spatial are NULL\n");
                    if (wgs84_spatial != NULL)
                        OSRDestroySpatialReference(wgs84_spatial);
                }
                else {
                    // Set up transformation from original datum to
                    // wgs84 datum.
                    transformH = OCTNewCoordinateTransformation(
                        spatial, wgs84_spatial);

                    if (transformH == NULL) {
                        // Couldn't create transformation object
fprintf(stderr,"Couldn't create transformation object\n");
                    }
                    else {
                        // We're good.  Perform the transform to
                        // WGS84 coordinates.
                        double x[2];
                        double y[2];

                        x[0] = file_MinX;
                        x[1] = file_MaxX;
                        y[0] = file_MinY;
                        y[1] = file_MaxY;

fprintf(stderr,"Before: %f,%f\t%f,%f\n",
x[0],y[0],
x[1],y[1]);

//                        if (OCTTransform(transformH,
//                            2,
//                            x,
//                            y,
//                            NULL) == OGRERR_NONE) {

                        {
                            int result;

                            result = OCTTransform(transformH, 2, x, y, NULL);
                            
fprintf(stderr," After: %f,%f\t%f,%f\tresult=%d\n",
x[0],y[0],
x[1],y[1],result);
 
// I get a result of '1'.  Should be '0'?.  Not sure why yet.  The
// simple C example on the OGR pages shows '1' as the proper success
// value, 0 as an error.

                            if (result) {
// Debug:  Don't add them to the index so that we can experiment
// with datum translation and such.
#ifndef WE7U
                                index_update_ll(filenm,    // Filename only
                                    y[0],  // Bottom
                                    y[1],  // Top
                                    x[0],  // Left
                                    x[1]); // Right
#endif  // WE7U
                            }
                        }
                    }
                    if (wgs84_spatial != NULL)
                        OSRDestroySpatialReference(wgs84_spatial);
                }
            }
        }
        else if (local && !first_extents) {
            // Convert to geographic/WGS84?  How?

fprintf(stderr, "Found local coordinate system.  Skipping indexing\n");

        }
        else {
            // Abandon all hope, ye who enter here!  Either we don't
            // have any extents, or we don't have a geographic,
            // projected, or local coordinate system.  We don't have
            // a clue how to index this dataset...
        }

/*
        // For now, set the index to be the entire world to get
        // things up and running.
        index_update_ll(filenm,    // Filename only
             -90.0,  // Bottom
              90.0,  // Top
            -180.0,  // Left
             180.0); // Right
*/

        // Close data source
        OGR_DS_Destroy( datasource );

        return; // Done indexing the file
/////////////////////////////////////////////////////////////////////
    }

 
    // Find out what type of file we're dealing with:
    // This reports "TIGER" for the tiger driver, "ESRI Shapefile"
    // for Shapefiles.
    // 
    ptr = OGR_Dr_GetName(driver);
    fprintf(stderr,"\n%s: ", ptr);

    // This one returns the name/path.  Less than useful since we
    // should already know this.
    ptr = OGR_DS_GetName(datasource);
    fprintf(stderr,"%s\n", ptr);

 

    // If we're going to write, we need to test the capability using
    // these functions:
    // OGR_Dr_TestCapability(); // Does Driver have write capability?
    // OGR_DS_TestCapability(); // Can we create new layers?



// Hard-coded line attributes, currently used for the entire drawing
// process.
(void)XSetLineAttributes (XtDisplay (w), gc, 1, LineSolid, CapButt,JoinMiter);
(void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]);  // black



// Optimization:  Get the envelope for each layer if it's not an
// expensive operation.  Skip the layer if it's completely outside
// our viewport.
 
    // Loop through all layers in the data source.
    //
    numLayers = OGR_DS_GetLayerCount(datasource);
    for(i=0; i<numLayers; i++)
    {
        OGRLayerH layer;
//        int j;
//        int numFields;
        OGRFeatureH feature;
//        OGRFeatureDefnH layerDefn;
        OGREnvelope psExtent;  
        OGRSpatialReferenceH spatial;
        int extents_found = 0;



        if (interrupt_drawing_now) {
            // Close data source
            OGR_DS_Destroy( datasource );
            return;
        }

        layer = OGR_DS_GetLayer( datasource, i );
        if (layer == NULL)
        {
            fprintf(stderr,
                "Unable to open layer %d of %s\n",
                i,
                full_filename);

            // Close data source
            OGR_DS_Destroy( datasource );
            return;
        }

        // Test the capabilities of the layer so that we know the
        // best way to access it.
        //
        // OGR_L_TestCapability:
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
        fprintf(stderr," Layer %02d: ", i);
        if (OGR_L_TestCapability(layer, OLCRandomRead)) {
            fprintf(stderr, "Random Read, ");
        }
        if (OGR_L_TestCapability(layer, OLCFastSpatialFilter)) {
            fprintf(stderr,
                "Fast Spatial Filter, ");
        }
        if (OGR_L_TestCapability(layer, OLCFastFeatureCount)) {
            fprintf(stderr,
                "Fast Feature Count, ");
        }
        if (OGR_L_TestCapability(layer, OLCFastGetExtent)) {
            fprintf(stderr,
                "Fast Get Extent, ");
        }

        // Query the coordinate system.  Need to have the extents in
        // WGS84 lat/long coordinate system in order to compute the
        // extents properly.
        //
        spatial = OGR_L_GetSpatialRef(layer);
        if (spatial) {
            const char *temp;
            int geographic = 0;
            int projected = 0;

            if (OSRIsGeographic(spatial)) {
                fprintf(stderr,"Geographic Coord, ");
                geographic++;
            }
            else if (OSRIsProjected(spatial)) {
                fprintf(stderr,"Projected Coord, ");
                projected++;
            }
            else {
                fprintf(stderr,"Local Coord, ");
            }

            // PROJCS, GEOGCS, DATUM, SPHEROID, PROJECTION
            //
            temp = OSRGetAttrValue(spatial, "DATUM", 0);
            fprintf(stderr,"DATUM: %s, ", temp);

            if (projected) {
                temp = OSRGetAttrValue(spatial, "PROJCS", 0);
                fprintf(stderr,"PROJCS: %s, ", temp);
 
                temp = OSRGetAttrValue(spatial, "PROJECTION", 0);
                fprintf(stderr,"PROJECTION: %s, ", temp);
            }

            temp = OSRGetAttrValue(spatial, "GEOGCS", 0);
            fprintf(stderr,"GEOGCS: %s, ", temp);

            temp = OSRGetAttrValue(spatial, "SPHEROID", 0);
            fprintf(stderr,"SPHEROID: %s, ", temp);

        }
        else {
            fprintf(stderr,"No Spatial Info, ");
        }

        // Get the extents for this layer.  OGRERR_FAILURE means
        // that the layer has no spatial info or that it would be
        // an expensive operation to establish the extent.
        //OGRErr OGR_L_GetExtent(OGRLayerH hLayer, OGREnvelope *psExtent, int bForce);
        if (OGR_L_GetExtent(layer, &psExtent, FALSE) != OGRERR_FAILURE) {
            // We have extents.  Check whether any part of the layer
            // is within our viewport.
            fprintf(stderr, "Extents obtained.");
            extents_found++;
        }
//        else {
//            if (OGR_L_GetExtent(layer, &psExtent, TRUE) != OGRERR_FAILURE) {
//                fprintf(stderr, "Extents obtained via FORCE.");
//                extents_found++;
//            }
//            else {
//                fprintf(stderr, "Extents are not available even with FORCE.");
//            }
//        }
        fprintf(stderr, "\n");
        if (extents_found) {
            fprintf(stderr,
                "  MinX: %f, MaxX: %f, MinY: %f, MaxY: %f\n",
                psExtent.MinX,
                psExtent.MaxX,
                psExtent.MinY,
                psExtent.MaxY);
        }


        // Dump info about this layer
//        layerDefn = OGR_L_GetLayerDefn( layer );
//        if (layerDefn != NULL) {
//            numFields = OGR_FD_GetFieldCount( layerDefn );

//            fprintf(stderr,"\n===================\n");
//            fprintf(stderr,"Layer %d: '%s'\n\n", i, OGR_FD_GetName(layerDefn));

//            for(j=0; j<numFields; j++)
//            {
//                OGRFieldDefnH fieldDefn;

//                fieldDefn = OGR_FD_GetFieldDefn( layerDefn, j );
//                fprintf(stderr," Field %d: %s (%s)\n", 
//                       j, OGR_Fld_GetNameRef(fieldDefn), 
//                       OGR_GetFieldTypeName(OGR_Fld_GetType(fieldDefn)));
//            }
//            fprintf(stderr,"\n");
//        }

// Here we need to convert to WGS84 and lat/long if necessary (using
// OGRCoordinateTransformation class and PROJ.4), then start
// plotting the points/lines/whatever.
// Query the geometry type using OGRFeatureDefn::GetGeomType(),
// which returns an OGRwkbGeometryType object.
// OGRLayer::GetNextFeature() will return one feature at a time from
// a layer.  Can also install a spatial filter first to limit what
// it returns.  OGRLayer::SetSpatialFilter().

/*
    {
        // Report the projection
        OGRSpatialReferenceH hSRS = OGR_L_GetSpatialRef(layer);
        char *pszProjection;
        char *pszPrettyWkt = NULL;


fprintf(stderr,"1\n");
        pszProjection = (char *)GDALGetProjectionRef(hDataset);
        if (OSRImportFromWkt(hSRS, &pszProjection) == CE_None) {

fprintf(stderr,"2\n");
            OSRExportToPrettyWkt(hSRS, &pszPrettyWkt, FALSE);
fprintf(stderr,"3\n");
            fprintf(stderr,"Coordinate System is: %s\n", pszPrettyWkt);
            CPLFree(pszPrettyWkt);
fprintf(stderr,"4\n");
        }
    }
*/

// Optimization:  Get the envelope for each feature, skip the
// feature if it's completely outside our viewport.

        // Loop through all of the features in the layer.
        //
        while ( (feature = OGR_L_GetNextFeature( layer )) != NULL ) {
            OGRGeometryH shape;
            int num, ii;
            double X1, Y1, Z1, X2, Y2, Z2;
            int polygon = 0;
//            OGRSpatialReferenceH spatial;
 

            if (interrupt_drawing_now) {
               if (feature != NULL)
                   OGR_F_Destroy( feature );
                // Close data source
                OGR_DS_Destroy( datasource );
                return;
            }

            if (feature == NULL) {
                continue;
            }

            //OGR_F_DumpReadable( feature, stderr );
            // Get a handle to the shape itself
            shape = OGR_F_GetGeometryRef(feature);
            if (shape == NULL) {
                OGR_F_Destroy( feature );
                continue;
            }

            //OGR_G_DumpReadable(shape, stderr, "Shape: ");

// We could either call OGR_G_GetEnvelope() here and calculate for
// ourselves it if is in our viewport, or we could set a filter and
// let the library pass us only those that fit.

// Causes segfaults on some tiger files
//            spatial = OGR_G_GetSpatialReference(shape);

            // This works for a point or a linestring only.
// Causes segfaults on some tiger files
            num = OGR_G_GetPointCount(shape);

            if (num == 0) {
                // Get number of elements (polygons)
                num = OGR_G_GetGeometryCount(shape);
//                fprintf(stderr,"Polygon: Number of elements: %d\n",num);
                if (num) {
                    polygon++;
                }
            }
            else {
//                fprintf(stderr,"Point/Line: Number of points: %d\n",num);
            }

/*
            // Print out the points
            for (ii=0; ii < num; ii++) {
                OGR_G_GetPoint(shape,
                    ii,
                    &X1,
                    &Y1,
                    &Z1);
                fprintf(stderr,"%f\t%f\t%f\n",pdfX,pdfY,pdfZ);
            }
*/

            // If point or line feature, draw in normal manner.
            // If polygon feature, do we do the "rotation one way =
            // fill, rotation the other way = hole" thing?

// At this point it'd be nice to either have all of the coordinates
// in WGS84 lat/long, or WGS84 Xastir coordinate system.  It'd be
// very nice if the coordinate transformation calls could do the
// latter one for us.  We'd then just draw the darn things.

            if (num > 0) {
                if (!polygon) { // Draw lines/points

                    // Get the first point
                    OGR_G_GetPoint(shape,
                        0,
                        &X2,
                        &Y2,
                        &Z2);

                    for (ii = 1; ii < num; ii++) {

                        X1 = X2;
                        Y1 = Y2;
                        Z1 = Z2;

                        // Get the next point
                        OGR_G_GetPoint(shape,
                            ii,
                            &X2,
                            &Y2,
                            &Z2);

                        draw_vector_ll(da,
                            (float)Y1,
                            (float)X1,
                            (float)Y2,
                            (float)X2,
                            gc,
                            pixmap);
                    }
                }
                else {  // Draw polygons
                }
            }
            OGR_F_Destroy( feature );
        }
        // No need to free layer handle, it belongs to the datasource
    }
    // Close data source
    OGR_DS_Destroy( datasource );
}



#endif // HAVE_LIBGDAL


