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
//#include "main.h"
//#include "datum.h"
//#include "draw_symbols.h"
//#include "rotated.h"
//#include "color.h"
//#include "xa_config.h"


#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }



#ifdef HAVE_LIBGDAL

#warning
#warning GDAL/OGR library support not implemented yet!
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
// This is very nearly verbatim the example C-API code off the OGR
// web pages.  We are only printing out the attribute field names so
// far, but at least we are exercising the OGR draw code.  We
// currently call this function from the draw_shapefile_map()
// function, just so that it gets called and we can play with it.
//
void draw_ogr_map(Widget w,
                   char *dir,
                   char *filenm,
                   alert_entry *alert,
                   u_char alert_color,
                   int destination_pixmap,
                   int draw_filled) {

    OGRDataSourceH datasource;
    int i, numLayers;
    char full_filename[300];


    xastir_snprintf(full_filename,
        sizeof(full_filename),
        "%s/%s",
        dir,
        filenm);

    /* Register all OGR drivers */
    OGRRegisterAll();

    /* Open data source */
    datasource = OGROpen(full_filename, 0 /* bUpdate */, NULL);

    if (datasource == NULL)
    {
        fprintf(stderr,"Unable to open %s\n", full_filename);
        return;
    }

    /* Loop through layers and dump their contents */

    numLayers = OGR_DS_GetLayerCount(datasource);
    for(i=0; i<numLayers; i++)
    {
        OGRLayerH layer;
        int j, numFields;
        OGRFeatureH feature;
        OGRFeatureDefnH layerDefn;

        layer = OGR_DS_GetLayer( datasource, i );

        /* Dump info about this layer */
        layerDefn = OGR_L_GetLayerDefn( layer );
        numFields = OGR_FD_GetFieldCount( layerDefn );

//        fprintf(stderr,"\n===================\n");
//        fprintf(stderr,"Layer %d: '%s'\n\n", i, OGR_FD_GetName(layerDefn));

        for(j=0; j<numFields; j++)
        {
            OGRFieldDefnH fieldDefn;

            fieldDefn = OGR_FD_GetFieldDefn( layerDefn, j );
//            fprintf(stderr," Field %d: %s (%s)\n", 
//                   j, OGR_Fld_GetNameRef(fieldDefn), 
//                   OGR_GetFieldTypeName(OGR_Fld_GetType(fieldDefn)));
        }
//        fprintf(stderr,"\n");



// Here we need to convert to WGS84 if necessary (using
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



        // Dump each feature individually
//        if ( (feature = OGR_L_GetNextFeature( layer )) != NULL ) {
        while ( (feature = OGR_L_GetNextFeature( layer )) != NULL ) {
            OGRSpatialReferenceH spatial;
            OGRGeometryH shape;
            int num, ii;
            double X1, Y1, Z1, X2, Y2, Z2;
            int polygon = 0;

//            OGR_F_DumpReadable( feature, stderr );

            // Get a handle to the shape itself
            shape = OGR_F_GetGeometryRef(feature);
//            OGR_G_DumpReadable(shape, stderr, "Shape: ");

// We could either call OGR_G_GetEnvelope() here and calculate for
// ourselves it if is in our viewport, or we could set a filter and
// let the library pass us only those that fit.

            spatial = OGR_G_GetSpatialReference(shape);

            // This works for a point or a linestring only.
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

(void)XSetLineAttributes (XtDisplay (w), gc, 1, LineSolid, CapButt,JoinMiter);
(void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]);  // black

                if (!polygon) {
                    // Draw lines/points

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
                else {
                    // Draw polygons
                }
            }
            OGR_F_Destroy( feature );
        }

        /* No need to free layer handle, it belongs to the datasource */
    }

    /* Close data source */
    OGR_DS_Destroy( datasource );
}



#endif // HAVE_LIBGDAL


