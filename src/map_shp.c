/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2026 The Xastir Group
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
 * shape_ring_direction() function in this file.
 *
 * DBFAWK TODO:
 *  - reload .dbfawk's when they've changed (or maps are reindexed)
 *  - scale line widths based on zoom level (see city_flag, for example)
 *  - allow multiple font sizes (font_size=small|medium|large|huge)
 *  - allow multiple font faces?
 *  - allow setting of map layer for individual shapes
 *  - transparency
 *  - allow setting fill_style (solid, stippled, etc.)
 *  - do more config/ *.dbfawk files!
 *
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
#include <limits.h>

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
#include "globals.h"
#include "maps.h"
#include "alert.h"
#include "util.h"
#include "main.h"
#include "db_funcs.h"
#include "datum.h"
#include "draw_symbols.h"
#include "rotated.h"
#include "color.h"
#include "xa_config.h"

#ifdef HAVE_LIBSHP
  #include "awk.h"
  #include "dbfawk.h"
#ifdef HAVE_SHAPEFIL_H
  #include <shapefil.h>
#else   // HAVE_SHAPEFIL_H
  #ifdef HAVE_LIBSHP_SHAPEFIL_H
    #include <libshp/shapefil.h>
  #else   // HAVE_LIBSHP_SHAPEFIL_H
    #error HAVE_LIBSHP defined but no corresponding include defined
  #endif // HAVE_LIBSHP_SHAPEFIL_H
#endif // HAVE_SHAPEFIL_H

#include <rtree/index.h>
#include "shp_hash.h"

// Must be last include file
#include "leak_detection.h"

// typedef needed in forward decls
typedef struct _label_string
{
  char   label[50];
  int    found;
  struct _label_string *next;
} label_string;

// forward declarations of functions local to this file
// It's OK for this to appear *after* "leak_detection.h" because
// it contains nothing that interferes with that file.
#include "map_shp_fwd.h"

// RTrees are used as a spatial index for shapefiles.  We can search them
// for shapes that intersect our viewport, and only read those from the
// file.
// RTree_hitarray is filled in when we do an rtree search.  the index is used
// to keep track of how many we've found so far while searching, or how
// many total have been found after the search is done.
static int *RTree_hitarray=NULL;
int RTree_hitarray_size=0;
int RTree_hitarray_index=0;


//This trivial routine is used by the RTreeSearch as a callback when it finds
// a match.
int RTreeSearchCallback(int id, void* UNUSED(arg) )
{
  if (!RTree_hitarray)
  {
    RTree_hitarray = (int *)malloc(1000*sizeof(int));
    RTree_hitarray_size=1000;
  }
  if (RTree_hitarray_size <= RTree_hitarray_index)
  {
    int *ptr;
    ptr = realloc(RTree_hitarray, (RTree_hitarray_size+1000)*sizeof(int));
    CHECKREALLOC(ptr);  // fatal error if we can't get 'em :-(
    RTree_hitarray=ptr;
    RTree_hitarray_size += 1000;
  }


  RTree_hitarray[RTree_hitarray_index++]=id-1;
  return 1;  // signal to keep searching for more matches
}





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
                          int add_timestamp, char * shape_label)
{

  SHPHandle my_shp_handle;
  SHPObject *my_object;
  DBFHandle my_dbf_handle;
  char timedatestring[101];
  int index;
  int max_objects = 1;
  char credit_string[] = "Created by Xastir, http://www.xastir.org";
  char temp_shapefile_name[MAX_FILENAME];
  char temp_prj_name[MAX_FILENAME];

  if (debug_level & 16)
  {
    fprintf(stderr,"create_shapefile_map\n");
    fprintf(stderr,"%s %s %d %d %d\n",
            dir,
            shapefile_name,
            type,
            quantity,
            add_timestamp);
  }

  if (quantity == 0)
  {
    // No reason to make a map if we don't have any points.
    return;
  }

  // Get the time/datestamp
  get_timestamp(timedatestring);

  if (add_timestamp)      // Prepend a timestamp to the filename
  {
    int ii;

    xastir_snprintf(temp_shapefile_name,
                    sizeof(temp_shapefile_name),
                    "%s%s_%s",
                    dir,
                    timedatestring,
                    shapefile_name);

    // Change spaces and colons to underlines
    for (ii = 0; ii < (int)strlen(temp_shapefile_name); ii++)
    {
      if (temp_shapefile_name[ii] == ' ' ||
          temp_shapefile_name[ii] == ':')
      {
        temp_shapefile_name[ii] = '_';
      }

    }
  }
  else    // Use the filename directly, no timestamp
  {
    xastir_snprintf(temp_shapefile_name,
                    sizeof(temp_shapefile_name),
                    "%s%s",
                    dir,
                    shapefile_name);
  }

  if (debug_level & 16)
  {
    fprintf(stderr, "Creating file %s\n", temp_shapefile_name);
  }

  // Create empty .shp/.shx/.dbf files
  //
  my_shp_handle = SHPCreate(temp_shapefile_name, type);
  my_dbf_handle = DBFCreate(temp_shapefile_name);

  // Check whether we were able to open these handles
  if ((my_shp_handle == NULL) || (my_dbf_handle == NULL))
  {
    // Probably write-protected directory
    fprintf(stderr, "Could not create shapefile %s\n",
            temp_shapefile_name);
    return;
  }

  // Write out a WKT in a .prj file to go with this shapefile.
  strcpy(temp_prj_name, temp_shapefile_name);
  temp_prj_name[sizeof(temp_prj_name)-1] = '\0';  // Terminate string
  strcat(temp_prj_name, ".prj");
  temp_prj_name[sizeof(temp_prj_name)-1] = '\0';  // Terminate string

  xastirWriteWKT(temp_prj_name);

  // Create the different fields we'll use to store the
  // attributes:
  //
  // Add a credits field and set the length.  Field 0.
  DBFAddField(my_dbf_handle, "Credits", FTString, strlen(credit_string) + 1, 0);
  //
  // Add a date/time field and set the length.  Field 1.
  DBFAddField(my_dbf_handle, "DateTime", FTString, strlen(timedatestring) + 1, 0);

  // Add a label field
  DBFAddField(my_dbf_handle, "Label", FTString, strlen(shape_label) + 1, 0);

  // Note that if were passed additional parameters that went
  // along with the lat/long/altitude points, we could write those
  // into the DBF file in the loop below.  We would have to change
  // from a polygon to a point filetype though.

  // Populate the files with objects and attributes.  Perform this
  // loop once for each object.
  //
  for (index = 0; index < max_objects; index++)
  {

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

    // Write the label string
    DBFWriteStringAttribute( my_dbf_handle,
                             index,
                             2,
                             shape_label);
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
void create_map_from_trail(char *call_sign)
{
  DataRow *p_station;


  // Find the station in our database.  Count the number of points
  // for that station first, then allocate some arrays to hold
  // that quantity of points.  Stuff the lat/long into the arrays
  // and then call create_shapefile_map().
  //
  if (search_station_name(&p_station, call_sign, 1))
  {
    int count;
    int ii;
    TrackRow *ptr;
    char temp[MAX_FILENAME];
    char temp2[MAX_FILENAME];
    double *x;
    double *y;
    double *z;
    char temp_base_dir[MAX_VALUE];

    count = 0;
    ptr = p_station->oldest_trackpoint;
    while (ptr != NULL)
    {
      count++;
      ptr = ptr->next;
    }

//fprintf(stderr, "Quantity of points: %d\n", count);

    if (count == 0)
    {
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
    while ((ptr != NULL) && (ii < count) )
    {

      // Convert from Xastir coordinates to lat/long

      // Convert to string
      convert_lon_l2s(ptr->trail_long_pos, temp, sizeof(temp), CONVERT_DEC_DEG);
      // Convert to double and stuff into array of doubles
      if (1 != sscanf(temp, "%lf", &x[ii]))
      {
        fprintf(stderr,"create_map_from_trail:sscanf parsing error\n");
      }
      // If longitude string contains "W", make the final
      // result negative.
      if (index(temp, 'W'))
      {
        x[ii] = x[ii] * -1.0;
      }

      // Convert to string
      convert_lat_l2s(ptr->trail_lat_pos, temp, sizeof(temp), CONVERT_DEC_DEG);
      // Convert to double and stuff into array of doubles
      if (1 != sscanf(temp, "%lf", &y[ii]))
      {
        fprintf(stderr,"create_map_from_trail:sscanf parsing error\n");
      }
      // If latitude string contains "S", make the final
      // result negative.
      if (index(temp, 'S'))
      {
        y[ii] = y[ii] * -1.0;
      }

      z[ii] = ptr->altitude;  // Altitude (meters), undefined=-99999

      ptr = ptr->next;
      ii++;
    }

    // Create a Shapefile from the APRS trail.  Write it into
    // "~/.xastir/tracklogs" and add a date/timestamp to the end.
    //
    xastir_snprintf(temp, sizeof(temp),
                    "%s/",
                    get_user_base_dir("tracklogs", temp_base_dir, sizeof(temp_base_dir)));

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
      1, // Add a timestamp to the front of the filename
      call_sign);

    // Free the storage that we malloc'ed
    free(x);
    free(y);
    free(z);
  }
  else    // Couldn't find the station of interest
  {
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
int shape_ring_direction ( SHPObject *psObject, int Ring )
{
  int nVertStart;
  int nVertCount;
  int iVert;
  float dfSum;
  int result;


  nVertStart = psObject->panPartStart[Ring];

  if( Ring == psObject->nParts-1 )
  {
    nVertCount = psObject->nVertices - psObject->panPartStart[Ring];
  }
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
  {
    result = 1;
  }
  else if (dfSum > 0.0)
  {
    result = -1;
  }
  else
  {
    result = 0;
  }

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
 * Shapefiles.  Rendering rules for any shapefile are provided by
 * an associated "awk-like" DBFAWK file.
 *
 * We handle the "hole" drawing in polygon shapefiles, where one
 * direction around the ring means a fill, and the other direction
 * means a hole in the polygon.
 *
 * If alert is NULL, draw every shape that fits the screen.  If
 * non-NULL, draw only the shape that matches the zone number.
 *

 * Weather alerts are handled by having a dbfawk file that defines a
 * "key" variable.  The "key" matches a code in the DBF file that corresponds
 * to the weather area to alert on.   Which field is used to construct
 * the key depends on the file and whether NWS has changed the format.
 * See DBFAWK files named "nws*.dbfawk" in the config directory.

 * If we are processing a weather alert, we are getting passed an
 * alert_entry that identifies which named shape to search for, and we
 * find the shape in the dbf file that has a key that matches.

 * The alert entries are created by alert_build_list.  If we're doing
 * alerts, we're being called from load_alert_maps.

 * If we're not given an alert_entry, then we're to draw the whole map.

 **********************************************************/

static dbfawk_sig_info *Dbf_sigs = NULL;
static awk_symtab *Symtbl = NULL;
/* these have to be static since Symtbl is recycled between calls */
/* used to be static inside draw_shapefile_map, but moved here
   because they need to be shared among other functions */
static char     dbfsig[1024],dbffields[1024],name[64],key[64],sym[4];
static int      color,lanes,filled,pattern,display_level,min_display_level,label_level;
static int      fill_style,fill_color;
static int      fill_stipple;
static int label_color = 8;
static int font_size = FONT_DEFAULT;
static int label_method=0;
static double label_lon=0.0;
static double label_lat=0.0;

/* default dbfawk rule when no better signature match is found */
static awk_rule dbfawk_default_rules[] =
{
  {
    0,
    BEGIN,
    NULL,
    NULL,
    0,
    0,
    "dbfinfo=\"\"; key=\"\"; lanes=1; color=8; fill_color=13; fill_stipple=0; name=\"\"; filled=0; fill_style=0; pattern=0; display_level=2147483647; min_display_level=0; label_level=0",
    0,
    0
  },
};
#define dbfawk_default_nrules (sizeof(dbfawk_default_rules)/sizeof(dbfawk_default_rules[0]))
static dbfawk_sig_info *dbfawk_default_sig = NULL;

void draw_shapefile_map (Widget w,
                         char *dir,
                         char *filenm,
                         alert_entry * alert,
                         u_char alert_color,
                         int destination_pixmap,
                         map_draw_flags *mdf)
{

  DBFHandle       hDBF;
  SHPObject       *object;
  static XPoint   points[MAX_MAP_POINTS];
  char            file[MAX_FILENAME];  /* Complete path/name of image file */
  char            short_filenm[MAX_FILENAME];
  int             i, fieldcount, recordcount, structure, ring;
  SHPHandle       hSHP;
  int             nShapeType, nEntities;
  double          adfBndsMin[4], adfBndsMax[4];
  char            *sType;
  long            x,y;
  int             ok;
  int             numXPoints;
  int             polygon_hole_flag;
  int             *polygon_hole_storage;
  GC              gc_temp = NULL;
  int             gps_flag = 0;
  char            gps_label[100];
  int             gps_color = 0x0c;
  int             weather_alert_flag = 0;
  char            *filename;  // filename itself w/o directory
  int             found_shape = -1;
  int             ok_to_draw = 0;
  int             high_water_mark_index = 0;
  char            status_text[MAX_FILENAME];

  dbfawk_sig_info *sig_info = NULL;
  dbfawk_field_info *fld_info = NULL;


  int draw_filled_orig;
  int draw_filled;

  // Define hash table for label pointers
  label_string *label_hash[256];
  label_string *ptr2 = NULL;

  struct Rect viewportRect;
  shpinfo *si;
  int nhits;

  // pull this out of the map_draw_flags
  draw_filled = mdf->draw_filled;

  // Initialize the hash table label pointers
  for (i = 0; i < 256; i++)
  {
    label_hash[i] = NULL;
  }


  // We're going to change draw_filled a bunch if we've got Auto turned
  // on, but we have to check --- save this!
  draw_filled_orig=draw_filled;

  // Re-initialize all static render-control variables every time
  // through here.
  initialize_rendering_variables();

  if (Dbf_sigs == NULL)
  {
    Dbf_sigs = dbfawk_load_sigs(get_data_base_dir("config"),".dbfawk");
  }

  if (debug_level & 16)
    fprintf(stderr,"DBFAWK signatures %sfound in %s.\n",
            (Dbf_sigs)?" ":"NOT ",get_data_base_dir("config"));

  dbfawk_default_sig = initialize_dbfawk_default_sig();

  // We don't draw the shapes if alert_color == -1
  if (alert_color != 0xff)
  {
    ok_to_draw++;
  }


  xastir_snprintf(file, sizeof(file), "%s/%s", dir, filenm);

  // Create a shorter filename for display
  short_filename_for_status(filenm, short_filenm, sizeof(short_filenm));

  // filename is the base name of filenm (path stripped off)
  filename = filenm;
  i = strlen(filenm);
  while ( (i >= 0) && (filenm[i] != '/') )
  {
    filename = &filenm[i--];
  }

  if (alert)
  {
    weather_alert_flag++;
  }

  // Check for ~/.xastir/GPS directory.  We set up the
  // labels and colors differently for these types of files.
  if (strstr(filenm,"GPS"))   // We're in the maps/GPS directory
  {
    gps_flag++;
  }

  // Open the .dbf file for reading.  This has the textual
  // data (attributes) associated with each shape.
  hDBF = DBFOpen( file, "rb" );
  if ( hDBF == NULL )
  {
    if (debug_level & 16)
    {
      fprintf(stderr,"draw_shapefile_map: DBFOpen(%s,\"rb\") failed.\n", file );
    }

    return;
  }
  if (debug_level & 16)
  {
    fprintf(stderr,"\n---------------------------------------------\nInfo for %s\n",filenm);
  }

  *dbfsig = '\0';
  fieldcount = dbfawk_sig(hDBF,dbfsig,sizeof(dbfsig));
  if (fieldcount == 0)
  {
    DBFClose( hDBF );   // Clean up open file descriptors
    return;     // Should have at least one field

  }
  recordcount = DBFGetRecordCount(hDBF);
  if (recordcount == 0)
  {
    DBFClose( hDBF );   // Clean up open file descriptors
    return;     // Should have at least one record
  }
  if (debug_level & 16)
  {
    fprintf(stderr,"%d Columns,  %d Records in file\n", fieldcount, recordcount);
    fprintf(stderr,"DBF signature: %s\n",dbfsig);
  }
  if (Dbf_sigs)     /* see if we have a .dbfawk file that matches */
  {
    sig_info = dbfawk_find_sig(Dbf_sigs,dbfsig,file);
    if (sig_info)
    {
      gps_flag = 0;  // trump gps_flag-- use dbfawk
    }
    if (!sig_info)
    {
      fprintf(stderr,"No DBFAWK signature for %s!  Using default.\n",filenm);
      sig_info = dbfawk_default_sig;
    }
  }
  else
  {
    sig_info = dbfawk_default_sig;
  }

  if (sig_info)           /* we've got a .dbfawk, so set up symtbl */
  {

    Symtbl = initialize_dbfawk_symbol_table(dbffields, sizeof(dbffields),
                                            &color, &lanes,
                                            name, sizeof(name),
                                            key, sizeof(key),
                                            sym, sizeof(sym),
                                            &filled, &fill_style,
                                            &fill_color, &fill_stipple,
                                            &pattern, &display_level,
                                            &min_display_level,
                                            &label_level, &label_color,
                                            &font_size, &label_method,
                                            &label_lon, &label_lat);

    if (awk_compile_program(Symtbl,sig_info->prog) < 0)
    {
      fprintf(stderr,"Unable to compile .dbfawk program\n");

      free_dbfawk_sig_info(sig_info);
      return;
    }
    awk_exec_begin(sig_info->prog); /* execute a BEGIN rule if any */

    /* find out which dbf fields we care to read */
    fld_info = dbfawk_field_list(hDBF, dbffields);
  }
  else                    /* should never be reached anymore! */
  {
    fprintf(stderr,"No DBFAWK signature for %s and no default!\n",filenm);
    return;
  }

  // Search for specific record if we're doing alerts (returns -1 if
  // alert is null or the shape is not found)
  found_shape = find_wx_alert_shape(alert, hDBF, recordcount,
                                    sig_info,fld_info);

  if (debug_level & 16)
  {
    fprintf(stderr,"Calling SHPOpen()\n");
  }

  // Open the .shx/.shp files for reading.
  // These are the index and the vertice files.
  hSHP = SHPOpen( file, "rb" );
  if( hSHP == NULL )
  {
    fprintf(stderr,"draw_shapefile_map: SHPOpen(%s,\"rb\") failed.\n", file );
    DBFClose( hDBF );   // Clean up open file descriptors

    free_dbfawk_infos(fld_info, sig_info);

    return;
  }


  // Get the extents of the map file
  SHPGetInfo( hSHP, &nEntities, &nShapeType, adfBndsMin, adfBndsMax );


  // Check whether we're indexing or drawing the map
  if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
       || (destination_pixmap == INDEX_NO_TIMESTAMPS) )
  {

    xastir_snprintf(status_text,
                    sizeof(status_text),
                    langcode ("BBARSTA039"),
                    short_filenm);
    statusline(status_text,0);       // Indexing ...

    // We're indexing only.  Save the extents in the index.
    index_update_ll(filenm,    // Filename only
                    adfBndsMin[1],  // Bottom
                    adfBndsMax[1],  // Top
                    adfBndsMin[0],  // Left
                    adfBndsMax[0],  // Right
                    1000);          // Default Map Level

    DBFClose( hDBF );   // Clean up open file descriptors
    SHPClose( hSHP );

    free_dbfawk_infos(fld_info, sig_info);

    return; // Done indexing this file
  }
  else
  {
    xastir_snprintf(status_text,
                    sizeof(status_text),
                    langcode ("BBARSTA028"),
                    short_filenm);
    statusline(status_text,0);       // Loading ...
  }

  // We put this section AFTER the code that determines whether we're merely
  // indexing, so we don't bother to generate rtrees unless we're really
  // drawing the file.
  si=NULL;

  // Don't bother even looking at the hash if this shapefile is completely
  // contained in the current viewport.  We'll have to read every shape
  // in it anyway, and all we'd be doing is extra work searching the
  // RTree
  if (!map_inside_viewport_lat_lon(adfBndsMin[1],
                                   adfBndsMax[1],
                                   adfBndsMin[0],
                                   adfBndsMax[0]))
  {
    // we keep a hash of all shapefiles encountered so far (and not purged
    // due to inactivity).  Find the record of this shapefile in that
    // hash if it's there.
    si = get_shp_from_hash(file);
    if (!si)
    {
      // we don't have what we need, so generate the index and make
      // the hashtable entry
      add_shp_to_hash(file,hSHP); // this will index all the shapes in
                                  // an RTree and save the root in a
                                  // shpinfo structure
      si=get_shp_from_hash(file); // now get that structure
      if (!si)
      {
        fprintf(stderr,
                "Panic!  added %s, lost it already!\n",file);
        exit(1);
      }
    }
  }

  // we need this for the rtree search
  getViewportRect(&viewportRect);

  sType = getShapeTypeString(nShapeType);
  if ((strcmp(sType, "Multipoint")== 0) || (strcmp(sType, "Unknown")==0))
  {
    fprintf(stderr,"%s Shapefile format not implemented: %s\n",sType,file);

    DBFClose( hDBF );   // Clean up open file descriptors
    SHPClose( hSHP );

    free_dbfawk_infos(fld_info, sig_info);
    return;
  }

  if (debug_level & 16)
  {
    fprintf(stderr,"%s(%d), %d Records in file\n",sType,nShapeType,nEntities);
  }

  if (debug_level & 16)
    fprintf(stderr,"File Bounds: (%15.10g,%15.10g)\n\t(%15.10g,%15.10g)\n",
            adfBndsMin[0], adfBndsMin[1], adfBndsMax[0], adfBndsMax[1] );

  // Check the bounding box for this shapefile.  If none of the
  // file is within our viewport, we can skip the entire file.

  if (debug_level & 16)
  {
    fprintf(stderr,"Calling map_visible_lat_lon on the entire shapefile\n");
  }

  if (! map_visible_lat_lon(  adfBndsMin[1],  // Bottom
                              adfBndsMax[1],  // Top
                              adfBndsMin[0],  // Left
                              adfBndsMax[0]) )    // Right
  {
    if (debug_level & 16)
    {
      fprintf(stderr,"No shapes within viewport.  Skipping file...\n");
    }

    DBFClose( hDBF );   // Clean up open file descriptors
    SHPClose( hSHP );

    free_dbfawk_infos(fld_info, sig_info);

    return;     // The file contains no shapes in our viewport
  }


  // Set a default line width for all maps.  This will most likely
  // be modified for particular maps in later code.
  (void)XSetLineAttributes(XtDisplay(w), gc, 0, LineSolid, CapButt,JoinMiter);

  if (weather_alert_flag)
  {
    char xbm_path[MAX_FILENAME];
    unsigned int _w, _h;
    int _xh, _yh;
    int ret_val;

    // This GC is used for weather alerts (writing to the
    // pixmap: pixmap_alerts)
    (void)XSetForeground (XtDisplay (w), gc_tint, colors[(int)alert_color]);

    // GXcopy used here because we have been using stippling for
    // weather alerts since commit 88d579
    (void)XSetFunction(XtDisplay(w), gc_tint, GXcopy);

    // Get a pixmap that will be used to shade this alert area
    get_alert_xbm_path(xbm_path, sizeof(xbm_path), alert);

    // set the stipple GC to the pattern we found in the alert xbm
    (void)XSetLineAttributes(XtDisplay(w), gc_tint, 0, LineSolid, CapButt,JoinMiter);
    XFreePixmap(XtDisplay(w), pixmap_wx_stipple);
    ret_val = XReadBitmapFile(XtDisplay(w),
                              DefaultRootWindow(XtDisplay(w)),
                              xbm_path,
                              &_w,
                              &_h,
                              &pixmap_wx_stipple,
                              &_xh,
                              &_yh);

    if (ret_val != 0)
    {
      fprintf(stderr,"XReadBitmapFile() failed: Bitmap not found? %s\n",xbm_path);

      // We shouldn't exit on this one, as it's not so severe
      // that we should kill Xastir.  I've seen this happen
      // after very long runtimes though, so perhaps there's a
      // problem somewhere in the X11 server and/or it's
      // caching?
      //
      //exit(1);
    }
    else
    {
      // We successfully loaded the bitmap, so set the stipple
      // properly to use it.  Skip this part if we were
      // unsuccessful at loading the bitmap.
      (void)XSetStipple(XtDisplay(w), gc_tint, pixmap_wx_stipple);
    }

  } /* ...end if (weather_alert_flag) */

  // Now that we have the file open, we can read out the structures.
  // We can handle Point, PolyLine and Polygon shapefiles at the moment.



  HandlePendingEvents(app_context);
  if (interrupt_drawing_now)
  {
    DBFClose( hDBF );   // Clean up open file descriptors
    SHPClose( hSHP );
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

    free_dbfawk_infos(fld_info, sig_info);

    return;
  }

  // Now instead of looping over all the shapes, search for the ones that
  // are in our viewport and only loop over those

  if (weather_alert_flag)     // We're drawing _one_ weather alert shape
  {
    if (found_shape != -1)      // Found the record
    {
      // just in case we haven't drawn any real maps yet, allocate
      // the RTree hit array and add our found_shape to it as if we
      // had found it in an rtree...
      if (!RTree_hitarray)
      {
        RTree_hitarray = (int *)malloc(sizeof(int)*1000);
        RTree_hitarray_size=1000;
      }
      CHECKMALLOC(RTree_hitarray);
      RTree_hitarray[0]=found_shape;
      nhits=1;
    }
    else    // Didn't find the record
    {
      nhits=0;
    }
  }
  else    // Draw an entire Shapefile map
  {
    // if it isn't completely inside the viewport, select those shapes
    // in the file that intersect the viewport.  si will be non null
    // in this case.
    if (si)
    {
      RTree_hitarray_index=0;
      // the callback will be executed every time the search finds a
      // shape whose bounding box overlaps the viewport.
      // RTree_hitarray will contain the shape numbers of every shape
      // found, nhits will be how many there are.
      nhits = Xastir_RTreeSearch(si->root, &viewportRect,
                                 (void *)RTreeSearchCallback, 0);
    }
    else
    {
      // we read the entire shapefile
      nhits=nEntities;
    }
  }

  // only iterate over the hits found by RTreeSearch, not all of them
  for (RTree_hitarray_index=0; RTree_hitarray_index<nhits;
       RTree_hitarray_index++)
  {
    int skip_it = 0;
    int skip_label = 0;


    // Call HandlePendingEvents() every XX objects so that we
    // can interrupt map drawing if necessary.  Keep this at a
    // power of two for speed.  Bigger numbers speed up map
    // drawing at the expense of being able to interrupt map
    // drawing quickly.  HandlePendingEvents() drops to about
    // 0.5% CPU at 64 (according to gprof).  That appears to be
    // the break-even point where CPU is minimal.
    //
    if ( (RTree_hitarray_index % 64) == 0 )
    {

      HandlePendingEvents(app_context);
      if (interrupt_drawing_now)
      {
        DBFClose( hDBF );   // Clean up open file descriptors
        SHPClose( hSHP );
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
        free_dbfawk_infos(fld_info, sig_info);
        return;
      }
    }

    // here's where we decide which shape number we're currently processing.
    // We're either going through all of those found by an rtree search,
    // the one that pertains to a weather alert, or all of them sequentially.
    if (si)
    {
      structure=RTree_hitarray[RTree_hitarray_index];
    }
    else if ((weather_alert_flag && found_shape!=-1))
    {
      structure = RTree_hitarray[0];
    }
    else
    {
      structure = RTree_hitarray_index;
    }

    object = SHPReadObject( hSHP, structure );  // Note that each structure can have multiple rings

    if (object == NULL)
    {
      continue;  // Skip this iteration, go on to the next
    }

    // Fill in the boundary variables in the alert record.  We use
    // this info in load_alert_maps() to determine which alerts are
    // within our view, without having to open up the shapefiles to
    // get this info (faster).
    if (weather_alert_flag)
    {
      alert->top_boundary    = object->dfYMax;
      alert->left_boundary   = object->dfXMin;
      alert->bottom_boundary = object->dfYMin;
      alert->right_boundary  = object->dfXMax;
    }

    // Here we check the bounding box for this shape against our
    // current viewport.  If we can't see it, don't draw it.
    if ( map_visible_lat_lon( object->dfYMin,   // Bottom
                              object->dfYMax,   // Top
                              object->dfXMin,   // Left
                              object->dfXMax) )     // Right
    {

      const char *temp;
      int jj;
      int x0 = 0; // Used for computing label rotation
      int x1 = 0;
      int y0 = 0;
      int y1 = 0;


      if (debug_level & 16)
      {
        fprintf(stderr,"Shape %d is visible, drawing it.", structure);
        fprintf(stderr,"  Parts in shape: %d\n", object->nParts );    // Number of parts in this structure
      }

      if (debug_level & 16)
      {
        // Print the field contents
        for (jj = 0; jj < fieldcount; jj++)
        {
          if (fieldcount >= (jj + 1) )
          {
            temp = DBFReadStringAttribute( hDBF, structure, jj );
            if (temp != NULL)
            {
              fprintf(stderr,"%s, ", temp);
            }
          }
        }
        fprintf(stderr,"\n");
        fprintf(stderr,"Done with field contents\n");
      }
      if (sig_info)
      {
        dbfawk_parse_record(sig_info->prog,hDBF,fld_info,structure);
        if (debug_level & 16)
        {
          fprintf(stderr,"------\n");
          fprintf(stderr,"dbfawk parse of structure %d: ",structure);
          fprintf(stderr,"color=%d ",color);
          fprintf(stderr,"lanes=%d ",lanes);
          fprintf(stderr,"name=%s ",name);
          fprintf(stderr,"key=%s ",key);
          fprintf(stderr,"symbol=%s ",sym);
          fprintf(stderr,"filled=%d ",filled);
          fprintf(stderr,"fill_style=%d ",fill_style);
          fprintf(stderr,"fill_color=%d ",fill_color);
          fprintf(stderr,"fill_stipple=%d ",fill_stipple);
          fprintf(stderr,"pattern=%d ",pattern);
          fprintf(stderr,"display_level=%d ",display_level);
          fprintf(stderr,"min_display_level=%d ",min_display_level);
          fprintf(stderr,"label_level=%d ",label_level);
          fprintf(stderr,"label_color=%d\n",label_color);
        }
        /* set attributes */
        (void)XSetForeground(XtDisplay(w), gc, colors[color]);


        // Let the user decide whether to make the map
        // filled or unfilled via the Map Properties dialog.
        // This allows things like the NOAA counties map to
        // be used as a base map (filled) or as a vector
        // overlay map at the user's discretion.  The
        // choices available are:
        // 0: Global No-Fill.  Don't fill any polygons.
        // 1: Global Fill.  Fill all polygons.
        // 2: Auto.  dbfawk file, if present, takes over control.
        //
        if (debug_level & 16)
        {
          fprintf(stderr," draw_filled is %d\n",draw_filled_orig);
        }
        switch (draw_filled_orig)
        {

          case 0: // Global No-Fill (Vector)
            // Do nothing, user has chosen Global
            // No_Fill for this map.  The draw_filled
            // variable takes precedence.
            break;

          case 1: // Global Fill
            // Do nothing, user has chosen Global
            // Fill for this map.  The draw_filled
            // variable takes precedence.
            break;

          case 2: // Auto
          default:
            // User has chosen Auto Fill for this map,
            // so the Dbfawk file controls the fill
            // property.  If no dbfawk file, "filled"
            // will take on the default setting listed
            // in dbfawk_default_rules[] above.
            draw_filled = filled;
            break;
        }


        if (weather_alert_flag)
        {
          fill_style = FillStippled;
        }

        skip_it = (map_color_levels && ((scale_y > display_level) || (scale_y < min_display_level)));
        skip_label = (map_color_levels && (scale_y > label_level));

      }

      switch ( nShapeType )
      {


        case SHPT_POINT:
        case SHPT_POINTZ:
          // We hit this case once for each point shape in the file,
          // if that shape is within our viewport.
          if (debug_level & 16)
          {
            fprintf(stderr,"Found Point Shapefile\n");
          }

          if (!skip_it)
          {
            const char *temp = NULL;
            int ok = 1;

            if (map_labels)
            {
              temp = name;
            }
            // Convert point to Xastir coordinates
            ok = get_vertex_screen_coords(object, 0, &x, &y);

            if (ok == 1)
            {
              // default symbol if dbfawk doesn't set it
              char symbol_table = '/';
              char symbol_id = '.'; /* small x */
              char symbol_over = ' ';

              // Use symbol from dbfawk if given
              if (*sym)
              {
                symbol_table = sym[0];
                symbol_id = sym[1];
                symbol_over = sym[2];
              }
              // Fine-tuned the location here so that the middle of
              // the symbol would be at the proper pixel.
              symbol(w, 0, symbol_table, symbol_id, symbol_over, pixmap, 1, x-10, y-10, ' ');

              // Labeling of points done here
              // Fine-tuned this string so that it is to the right of
              // the symbol and aligned nicely.
              if (map_labels && !skip_label)
              {
                draw_nice_string(w, pixmap, 0, x+10, y+5, (char*)temp, 0xf, 0x10, strlen(temp));
              }
            }
          }
          break;



        case SHPT_ARC:
        case SHPT_ARCZ:
          // We hit this case once for each polyline shape in the
          // file, if at least part of that shape is within our
          // viewport.

          // Default in case we forget to set the line width later:
          (void)XSetLineAttributes (XtDisplay (w), gc, 0,
                                    LineSolid, CapButt,JoinMiter);


          // gps files in the GPS directory are treated specially to
          // handle an old pre-dbfawk use case.
          if (gps_flag)
            get_gps_color_and_label(filename, gps_label, sizeof(gps_label),
                                    &gps_color);

          // these will be used to determine if we label this feature
          int new_label = 1;
          int mod_number;

          if (ok_to_draw && !skip_it)
          {

            int nParts = object->nParts;

            if (nParts==0)
              nParts=1;     // but don't try to read panPartStart!

            // Read the vertices for each vector now
            // vectors may have multiple parts, draw them separately
            for (int part=0; part < nParts; part++)
            {
              int nVertices;
              int partStart;
              numXPoints = 0;
              if (nParts == 1)
              {
                nVertices =object->nVertices;
                partStart = 0;
              }
              else if (part < nParts-1)
              {
                partStart = object->panPartStart[part];
                nVertices = object->panPartStart[part+1] - partStart;
              }
              else
              {
                partStart = object->panPartStart[part];
                nVertices = object->nVertices - partStart;
              }

              // numXPoints winds up being the number of points we read into
              // the points array
              numXPoints = get_vertices_screen_coords_XPoints(object, partStart,
                                                 nVertices, points,
                                                 &high_water_mark_index);

              // Save the endpoints of the first line segment for
              // later use in label rotation
              x0=points[0].x;
              y0=points[0].y;
              x1=points[1].x;
              y1=points[1].y;
              // Reset these for each part, because we might have changed
              // them for the labels of the last part.
              set_shpt_arc_attributes(w, (gps_flag)?gps_color:color,
                                      (gps_flag)?3:((lanes)?lanes:1),
                                      (gps_flag)?LineOnOffDash:pattern);
              (void)XDrawLines(XtDisplay(w), pixmap, gc,
                               points, l16(numXPoints),
                               CoordModeOrigin);

              // draw a label
              temp = (gps_flag)?gps_label:name;
              if ( (temp != NULL)
                   && (strlen(temp) != 0)
                   && map_labels
                   && !skip_label )
              {
                x=points[0].x;
                y=points[0].y;

                // We only do this determination for the first part of
                // each arc.  If we label one part, we label them all.
                if (part == 0)
                {
                  // Set up the mod_number, which is used below to
                  // determine how many of each identical label are
                  // skipped at each zoom level.
                  mod_number = select_arc_label_mod();

                  // Check whether we've written out this string recently.
                  new_label = check_label_skip(label_hash, temp,
                                               mod_number, &skip_label);
                }

                if (!skip_label)    // Draw the string
                {
                  // Compute the label rotation angle, select color
                  float angle = (gps_flag)?(-90):get_label_angle(x0,x1,y0,y1);
                  int color_to_use=(gps_flag)?gps_color:label_color;

                  (void)draw_rotated_label_text(w, (int)angle, x, y,
                                                strlen(temp),
                                                colors[color_to_use],
                                                (char *)temp,
                                                font_size);

                }

                if (new_label)
                  add_label_to_label_hash(label_hash, temp);
              }
            }
          }
          break;



        case SHPT_POLYGON:
        case SHPT_POLYGONZ:

          if (debug_level & 16)
          {
            fprintf(stderr,"Found Polygons\n");
          }

          // Set the stipple now.  need to do here, because if
          // done earlier the labels get stippled, too, and if only done
          // right when we draw the polygon, single-part shapes with holes
          // won't stippled properly, because this stipple applies to that
          // GC.
          set_shpt_polygon_fill_stipple(w, fill_style, fill_stipple,
                                        draw_filled);
          polygon_hole_flag = 0;

          // Allocate storage for a flag for each ring in
          // this Shape.
          // !!Remember to free this storage later!!
          polygon_hole_storage = (int *)malloc(object->nParts*sizeof(int));
          CHECKMALLOC(polygon_hole_storage);

          // Determine whether we have any holes, and set flags in
          // polygon_hole_storage for ring that is a hole:
          polygon_hole_flag = preprocess_shp_polygon_holes(object,
                                                        polygon_hole_storage);



          // reset polygon hole flag if we aren't actually going to need
          // to go through the hole math and clip region setting
          // But we still use the poly_hole_storage, because we'll draw
          // the holes with dashed lines if we're not filling the polygons,
          // so we do the hole determination regardless of fill settings.
          if (!map_color_fill || !draw_filled)
          {
            polygon_hole_flag = 0;
          }

          if (polygon_hole_flag)
          {
            // set up a graphics context that has a "swiss cheese"
            // rectangle with all the holes cut out of it as its clipping
            // mask.  We'll draw our filled polygons in that GC and the holes
            // won't get filled
            gc_temp = get_hole_clipping_context(w,object,
                                                polygon_hole_storage,
                                                &high_water_mark_index);
          }

          // Read the vertices for each ring in this Shape
          int nParts = object->nParts;
          if (nParts == 0)
          {
            nParts = 1;  // but panPartStart is null, so don't read it!
          }
          for (ring = 0; ring < nParts; ring++ )
          {
            int nVertices;
            int partStart;
            if (nParts == 1)
            {
              nVertices = object->nVertices;
              partStart = 0;
            }
            else if ( (ring+1) < object->nParts)
            {
              partStart = object->panPartStart[ring];
              nVertices = object->panPartStart[ring+1] - partStart;
            }
            else
            {
              partStart = object->panPartStart[ring];
              nVertices = object->nVertices-partStart;
            }

            numXPoints = get_vertices_screen_coords_XPoints(object,
                                                   partStart, nVertices,
                                                   points,
                                                   &high_water_mark_index);

            if ( (numXPoints >= 3)
                 && (ok_to_draw && !skip_it)
                 && ( !draw_filled || !map_color_fill || (draw_filled && polygon_hole_storage[ring] == 0) ) )
            {
              // We have a polygon to draw!
              if ((!draw_filled || !map_color_fill) && polygon_hole_storage[ring] == 1)
              {
                // We have a hole drawn as unfilled.
                draw_polygon_boundary_dashed(w,color,points,numXPoints);
              }
              else if (!weather_alert_flag)
              {
                // it's not a weather alert, so draw it, filling if
                // necessary and taking into account any holes.

                // User requested filled polygons with stippling.
                // Set the stipple now.  need to do here, because
                // if not done inside the loop, only the first part of the
                // multi-part polygon gets stippled!
                set_shpt_polygon_fill_stipple(w, fill_style, fill_stipple,
                                        draw_filled);
                draw_filled_polygon(w,
                                    (polygon_hole_flag)?gc_temp:gc,
                                    points, numXPoints, color, fill_color,
                                    lanes, pattern,
                                    (map_color_fill && draw_filled));
              }
              else if (weather_alert_flag)
              {
                // If we're a weather alert, we're assuming the shape
                // is simple with no holes.  Draw the filled polygon
                // and the polygon border, all of which will be
                // stippled with an alert pattern because we already
                // set that up in gc_tint.
                draw_wx_polygon(w, points, numXPoints);
              }
              else
              {
                // the last two elseifs cover all cases not covered by
                // the first if, and therefore this is unreachable
                // and ought to be removed
                fprintf(stderr,"How have we gotten to this strange place?\n");
              }
            }
          }
          // Free the storage that we allocated to hold
          // the "hole" flags for the shape.
          free(polygon_hole_storage);

          if (polygon_hole_flag)
          {
            //Free the temporary GC that we may have used to
            //draw polygons using the clip-mask:
            XFreeGC(XtDisplay(w), gc_temp);
          }


          // Done with drawing shapes, now draw labels

          // Set fill style back to defaults, or labels would get
          // stippled along with polygons if we haven't already reset it.
          // At the moment, draw_filled_polygon *does* reset it, and
          // draw_wx_polygon only frobs gc_tint, but it doesn't hurt to
          // make sure.
          XSetFillStyle(XtDisplay(w), gc, FillSolid);

          if ( (strlen(name) != 0)
               && map_labels
               && !skip_label )
          {
            float label_lon, label_lat;

            if (debug_level & 16)
            {
              fprintf(stderr,"I think I should display labels\n");
            }
            ok = 1;

            /* TODO:  consider other label point options */
            /* for now, this function just gives us the center of the
             * bounding box
             */
            choose_polygon_label_point(object,&label_lon, &label_lat);
            ok=convert_ll_to_screen_coords(&x, &y, label_lon, label_lat);
            if (ok == 1 && ok_to_draw)
            {
              if (debug_level & 16)
              {
                fprintf(stderr,
                        "  displaying label %s with color %x\n",
                        name,label_color);
              }
              (void)draw_centered_label_text(w,
                                             -90,
                                             x,
                                             y,
                                             strlen(name),
                                             colors[label_color],
                                             (char *)name,
                                             font_size);
            }
          }
          break;

        case SHPT_MULTIPOINT:
        case SHPT_MULTIPOINTZ:
          // Not implemented.
          fprintf(stderr,"Shapefile Multi-Point format files aren't supported!\n");
          break;

        default:
          // Not implemented.
          fprintf(stderr,"Shapefile format not supported: Subformat unknown (default clause of switch)!\n");
          break;

      }   // End of switch
    }
    SHPDestroyObject( object ); // Done with this structure
  }


  // Free our hash of label strings, if any.  Each hash entry may
  // have a linked list attached below it.
  for (i = 0; i < 256; i++)
  {
    ptr2 = label_hash[i];
    while (ptr2 != NULL)
    {
      label_hash[i] = ptr2->next;
      free(ptr2);
      ptr2 = label_hash[i];
    }
  }

  free_dbfawk_infos(fld_info, sig_info);

  DBFClose( hDBF );
  SHPClose( hSHP );

//    XmUpdateDisplay (XtParent (da));

  if (debug_level & 16)
  {
    fprintf(stderr,"High-Mark Index:%d\n",
            high_water_mark_index);
  }

  // Set fill style back to defaults
  XSetFillStyle(XtDisplay(w), gc, FillSolid);
}
// End of draw_shapefile_map()




// This function will delete any pre-loaded dbfawk sigs and clear Dbf_sigs
// This will trigger a  reload the first time a shapfile is redisplayed
void clear_dbfawk_sigs(void)
{
  if (Dbf_sigs )
  {
    dbfawk_free_sigs(Dbf_sigs);
    Dbf_sigs = NULL;
  }
}

void free_dbfawk_infos(dbfawk_field_info *fld_info, dbfawk_sig_info *sig_info)
{
  dbfawk_free_info(fld_info);
  free_dbfawk_sig_info(sig_info);
}

void free_dbfawk_sig_info(dbfawk_sig_info *sig_info)
{
  if (sig_info != NULL && sig_info != dbfawk_default_sig  && (sig_info->sig == NULL))
  {
    dbfawk_free_sigs(sig_info);
  }
}




// Initializes the global "dbfawk_default_sig" if uninitialized.
// do nothing if already initialized.
// No matter what, return the pointer
dbfawk_sig_info *initialize_dbfawk_default_sig(void)
{
  if (dbfawk_default_sig == NULL)
  {
    /* set up default dbfawk when no sig matches */
    // This one is ok to leave allocated, as it gets malloc'ed
    // once during each runtime and then gets left alone.  We
    // don't need to free it.
    dbfawk_default_sig = calloc(1,sizeof(dbfawk_sig_info));
    CHECKMALLOC(dbfawk_default_sig);

    // Calls awk_new_program which allocates memory.  Again, we
    // don't need to free this one, as it gets allocated only
    // once per runtime.
    dbfawk_default_sig->prog = awk_load_program_array(dbfawk_default_rules,dbfawk_default_nrules);
  }

  return dbfawk_default_sig;
}




// Allocate and set up a DBFAWK symbol table.
// Note that the symbol table is allocated ONCE the very first time
// any shapefile is rendered, and persists forever.  The pointers we
// use to set up the table must therefore be static and never point to
// a local variable.
//
// This function returns the pointer to the symbol table.
// if it was already initialized, just return the pointer
awk_symtab *initialize_dbfawk_symbol_table(char *dbffields, size_t dbffields_s,
                                           int *color, int *lanes,
                                           char *name, size_t name_s,
                                           char *key, size_t key_s,
                                           char *sym, size_t sym_s,
                                           int *filled,
                                           int *fill_style,
                                           int *fill_color, int *fill_stipple,
                                           int *pattern, int *display_level,
                                           int *min_display_level,
                                           int *label_level,
                                           int *label_color,
                                           int *font_size,
                                           int *label_method,
                                           double *label_lon,
                                           double *label_lat)
{
  if (!Symtbl)
  {
    Symtbl = awk_new_symtab();
    awk_declare_sym(Symtbl,"dbffields",STRING,dbffields,dbffields_s);
    awk_declare_sym(Symtbl,"color",INT,color,sizeof(*color));
    awk_declare_sym(Symtbl,"lanes",INT,lanes,sizeof(*lanes));
    awk_declare_sym(Symtbl,"name",STRING,name,name_s);
    awk_declare_sym(Symtbl,"key",STRING,key,key_s);
    awk_declare_sym(Symtbl,"symbol",STRING,sym,sym_s);
    awk_declare_sym(Symtbl,"filled",INT,filled,sizeof(*filled));
    awk_declare_sym(Symtbl,"fill_style",INT,fill_style,sizeof(*fill_style));
    awk_declare_sym(Symtbl,"fill_color",INT,fill_color,sizeof(*fill_color));
    awk_declare_sym(Symtbl,"fill_stipple",INT,fill_stipple,sizeof(*fill_stipple));
    awk_declare_sym(Symtbl,"pattern",INT,pattern,sizeof(*pattern));
    awk_declare_sym(Symtbl,"display_level",INT,display_level,sizeof(*display_level));
    awk_declare_sym(Symtbl,"min_display_level",INT,min_display_level,sizeof(*min_display_level));
    awk_declare_sym(Symtbl,"label_level",INT,label_level,sizeof(*label_level));
    awk_declare_sym(Symtbl,"label_color",INT,label_color,sizeof(*label_color));
    awk_declare_sym(Symtbl,"font_size",INT,font_size,sizeof(*font_size));
    awk_declare_sym(Symtbl,"label_method",INT,label_method,sizeof(label_method));
    awk_declare_sym(Symtbl,"label_lon",FLOAT,label_lon,sizeof(*label_lon));
    awk_declare_sym(Symtbl,"label_lat",FLOAT,label_lat,sizeof(*label_lat));
  }

  return (Symtbl);
}




// We have an open DBF file (pointed to by hDBF), and we might be a
// weather alert.  Try to find a shape in the dbf file that has a key
// matching the alert's filename.
//
// returns -1 if shape not found or if passed a null alert pointer
/*
 * Weather alert dbfawk files set the "key" variable to the
 * appropriate search key for each record which is compared to the
 * alert->title[].  Use the key to find the record we need to alert on.
 */
int find_wx_alert_shape(alert_entry *alert, DBFHandle hDBF, int recordcount,
                        dbfawk_sig_info *sig_info, dbfawk_field_info *fld_info)
{
  int found_shape = -1;
  if (alert)
  {
    if (alert->index== -1)
    {
      int done = 0;
      int i;
      // Step through all records
      for( i = 0; i < recordcount && !done; i++ )
      {
        int keylen;
        char modified_title[50];

        dbfawk_parse_record(sig_info->prog,hDBF,fld_info,i);

        keylen = strlen(key);
        if (debug_level & 16)
        {
          static char old_key[4]; // Used to limit number of output lines in debug mode

          if (strncmp(old_key, key, 4))
          {
            fprintf(stderr,"dbfawk alert parse: record %d key=%s\n",
                    i,key);
            memcpy(old_key, key, sizeof(old_key));
          }
        }

        xastir_snprintf(modified_title, sizeof(modified_title), "%s", alert->title);

        // Tweak for RED_FLAG alerts:  If RED_FLAG alert
        // we've changed the 'Z' to an 'F' in our
        // alert->title already.  Change the 'F' back to a
        // 'Z' temporarily (modified_title) for our
        // compares.
        //
        if (modified_title[3] == 'F' && strncmp(alert->filename, "fz", 2) == 0)
        {
          modified_title[3] = 'Z';
        }

        // If match using keylen number of chars, try the
        // same match but using titlelen number of chars
        if (strncmp(modified_title,key,keylen) == 0)
        {
          int titlelen;

          titlelen = strlen(modified_title);

          // Try the same match with titlelen number of
          // chars
          if (strncmp(modified_title,key,titlelen) == 0)
          {

            found_shape = i;
            done++;
            if (debug_level & 16)
            {
              fprintf(stderr,"dbfawk alert found it: %d \n",i);
              fprintf(stderr,"Title %s, key %s\n",modified_title,key);

            }
          }
          else
          {
            // Found a match using keylen number of
            // characters, but it's not a match using
            // titlelen number of characters.
            if (debug_level & 16)
            {
              fprintf(stderr,
                      "dbfawk alert: match w/keylen, not w/titlelen: %s=%d %s=%d\n",
                      key,
                      keylen,
                      modified_title,
                      titlelen);
              fprintf(stderr,"Title %s, key %s\n",modified_title,key);
            }
          }
        }
      }
      alert->index = found_shape; // Fill it in 'cuz we just found it
    }
    else
    {
      // We've been here before and we already know the index into the
      // file to fetch this particular shape.
      found_shape = alert->index;
      if (debug_level & 16)
      {
        fprintf(stderr,"wx_alert: found_shape = %d\n",found_shape);
      }
    }
  }
  return (found_shape);
}




// this function fills in a Rect structure with the current viewport
// info
void getViewportRect(struct Rect *viewportRect)
{
  double rXmin, rYmin, rXmax,rYmax;
  get_viewport_lat_lon(&rXmin, &rYmin, &rXmax, &rYmax);
  viewportRect->boundary[0] = (RectReal) rXmin;
  viewportRect->boundary[1] = (RectReal) rYmin;
  viewportRect->boundary[2] = (RectReal) rXmax;
  viewportRect->boundary[3] = (RectReal) rYmax;
}




// Return a string corresponding to the name of a shape type
// This string is only used in debug output
char *getShapeTypeString(int nShapeType)
{
  char *sType;
  switch ( nShapeType )
  {
    case SHPT_POINT:
      sType = "Point";
      break;

    case SHPT_POINTZ:
      sType = "3D Point";
      break;

    case SHPT_ARC:
      sType = "Polyline";
      break;

    case SHPT_ARCZ:
      sType = "3D Polyline";
      break;

    case SHPT_POLYGON:
      sType = "Polygon";
      break;

    case SHPT_POLYGONZ:
      sType = "3D Polygon";
      break;

    case SHPT_MULTIPOINT:
      sType = "MultiPoint";
      break;

    default:
      sType = "Unknown";
      break;
  }
  return (sType);
}




// Find an xbm in our collection that matches the weather alert type.
// Will use "alert.xbm" if we can't find a more appropriate one.
//
// Note that we can define more alert files for other countries.  They just need to match
// the alert text that comes along in the NWS packet.
// Current alert files we have defined:
//      winter_storm.xbm *
//      snow.xbm
//      winter_weather.xbm *
//      flood.xbm
//      torndo.xbm *
//      red_flag.xbm
//      wind.xbm
//      alert.xbm (Used if no match to another filename)

// Alert texts we receive:
//      FLOOD
//      SNOW
//      TORNDO
//      WIND
//      WINTER_STORM
//      WINTER_WEATHER
//      RED_FLAG
//      SVRTSM (no file defined for this yet)
//      Many others.
void get_alert_xbm_path(char *xbm_path, size_t xbm_path_size, alert_entry *alert)
{
  // Attempt to open the alert filename:  <alert_tag>.xbm (lower-case alert text)
  // to detect whether we have a matching filename for our alert text.
  FILE *alert_fp = NULL;
  char xbm_filename[MAX_FILENAME];

  xastir_snprintf(xbm_filename, sizeof(xbm_filename), "%s", alert->alert_tag);

  // Convert the filename to lower-case
  to_lower(xbm_filename);

  // Construct the complete path/filename
  xastir_snprintf(xbm_path, xbm_path_size, "%s/%s.xbm",SYMBOLS_DIR, xbm_filename);

  // Try opening the file
  alert_fp = fopen(xbm_path, "rb");
  if (alert_fp == NULL)
  {
    // Failed to find a matching file:  Instead use the "alert.xbm" file
    xastir_snprintf(xbm_path, xbm_path_size, "%s/%s", SYMBOLS_DIR, "alert.xbm");
  }
  else
  {
    // Success:  Close the file pointer
    fclose(alert_fp);
  }
}




// If the file we're processing is in the GPS directory and has
// a color indicated in the file name, set gps_color to that.  Also
// set a label string based on the file name with the color stripped off.
// arguments:
//     filename: the base name of the file with all directory paths stripped
//               off.
//  the rest speak for themselves.
//
// This technique exists because WE7U said:
//   I'd like to be able to change the color of each GPS track for
//  each team in the field.  It'll help to differentiate the tracks
//   where they happen to cross.
//
// Alan Crosswell later wrote:
//    WITH_DBFAWK should handle this case too.  Need to add a color
//             attribute to the generated dbf file
//
// But these GPS tracks were generally downloaded from GPSMan, and so
// we had no control over the attributes in the dbf file, so Alan's
// suggestion wouldn't work unless we used shapelib code to modify it after
// the fact.
//
// Since commit 6bc21a, however, Xastir creates a per-file dbfawk file
// to go with every shapefile it creates from GPS data, so this
// filename-based technique is not really the recommended practice
// anymore.  Now, the recommended technique is to move the files
// out of the GPS directory where they got dumped and edit the per-file
// dbfawk to change color and other rendering details like labeling.

void get_gps_color_and_label(char *filename, char *gps_label,
                             size_t gps_label_size, int *gps_color)
{
  int jj;
  int done = 0;

  // Fill in the label we'll use later
  xastir_snprintf(gps_label, gps_label_size, "%s", filename);

  // Knock off the "_Color.shp" portion of the label.  Find the last
  // underline character and change it to an end-of-string.
  jj = strlen(gps_label);
  while ( !done && (jj > 0) )
  {
    if (gps_label[jj] == '_')
    {
      gps_label[jj] = '\0';   // Terminate it here
      done++;
    }
    jj--;
  }


  // Check for a color in the filename: i.e.  "Team2_Track_Red.shp"
  if (strstr(filename,"_Red.shp"))
  {
    *gps_color = 0x0c; // Red
  }
  else if (strstr(filename,"_Green.shp"))
  {
    *gps_color = 0x23; // Area Green Hi
  }
  else if (strstr(filename,"_Black.shp"))
  {
    *gps_color = 0x08; // black
  }
  else if (strstr(filename,"_White.shp"))
  {
    *gps_color = 0x0f; // white
  }
  else if (strstr(filename,"_Orange.shp"))
  {
    *gps_color = 0x62; // orange3 (brighter)
  }
  else if (strstr(filename,"_Blue.shp"))
  {
    *gps_color = 0x03; // cyan
  }
  else if (strstr(filename,"_Yellow.shp"))
  {
    *gps_color = 0x0e; // yellow
  }
  else if (strstr(filename,"_Purple.shp"))
  {
    *gps_color = 0x0b; // mediumorchid
  }
  else    // Default color
  {
    *gps_color = 0x0c; // Red
  }
}




// This function converts a lat/lon pair to screen coordinates
// It probably belongs in util.c
int convert_ll_to_screen_coords(long *x, long *y, float lon, float lat)
{
  int temp_ok;
  int ok;
  unsigned long my_lat, my_long;

  ok = 1;

  // Convert to Xastir coordinates
  temp_ok = convert_to_xastir_coordinates(&my_long,
                                          &my_lat,
                                          lon,
                                          lat);

  if (!temp_ok)
  {
    fprintf(stderr,"convert_ll_to_screen_coordinates: Problem converting from lat/lon\n");
    ok = 0;
    *x = 0;
    *y = 0;
  }
  else
  {
    convert_xastir_to_screen_coordinates(my_long, my_lat, x, y);
  }

  return ok;
}




// This function extracts all of the vertices from a shapefile object
// given a starting point and a number of vertices, and deposits them
// into the provided XPoint array.
// Returns the number of points converted
int get_vertices_screen_coords_XPoints(SHPObject *object, int partStart,
                                        int nVertices, XPoint *points,
                                        int *high_water_mark_index)
{
  int index = 0;
  for (int vertex = 0 ; vertex < nVertices; vertex++)
  {
    index = get_vertex_screen_coords_XPoint(
                                            object, vertex+partStart, points,
                                            index, high_water_mark_index);
  }
  return (index);
}




// This function extracts a single vertex from a shapefile object given
// its index in the vertex list of the SHPObject
// They will be deposited in the array of XPoints, converted to screen
// coordinates, at index given
// high_water_mark_index will be updated if needed
// we return the index of the next point that should be stored.
// This will not necessarily be one more than what we were given, if doing
// so would overrun the points array.
//
// high_water_mark represents the most points we've read in this file.  It
// is used only for debugging output.
int get_vertex_screen_coords_XPoint(SHPObject *object, int vertex, XPoint *points, int index, int *high_water_mark_index)
{
  int ok;
  long x, y;
  ok = get_vertex_screen_coords(object, vertex, &x, &y);
  if (ok == 1)
  {
    // XDrawLines uses 16-bit unsigned integers (shorts).
    // Make sure we stay within the limits.
    points[index].x = l16(x);
    points[index].y = l16(y);
    index++;
  }
  if (index > *high_water_mark_index)
  {
    *high_water_mark_index = index;
  }

  if (index >= MAX_MAP_POINTS)
  {
    index = MAX_MAP_POINTS - 1;
    fprintf(stderr,"Trying to overrun the XPoints array handling a shapefile, index=%d, dropping points to prevent overrun\n",index);
  }
  return index;
}


// this function gets a vertex's screen coordinates into variables x and y
int get_vertex_screen_coords(SHPObject *object, int vertex, long *x, long *y)
{
  return(convert_ll_to_screen_coords(x,y,
                                     object->padfX[vertex],
                                     object->padfY[vertex]));
}




// Select a "mod" number based on the y pixel scale that will be used to
// reduce the clutter of SHPT_ARC labels.
//
// Original comments:
// The goal here is to have one complete label visible on the screen
// for each road.  We end up skipping labels based on zoom level,
// which, if the road doesn't have very many segments, may end up
// drawing one label almost entirely off-screen.  :-(

// If we could check the first line segment to see if the label
// would be drawn off-screen, perhaps we could start drawing at
// segment #2?  We'd have to check whether there is a segment #2.
// Another possibility would be to shift the label on-screen.  Would
// this work for twisty/turny roads though?  I suppose, 'cuz they'd
// end up with more line segments and we could just draw at segment
// #2 in that case instead of shifting.

// Takes no arguments, because scale_y is a global variable
int select_arc_label_mod(void)
{
  int mod_number;
  if (scale_y == 1)
  {
    mod_number = 1;
  }
  else if (scale_y <= 2)
  {
    mod_number = 1;
  }
  else if (scale_y <= 4)
  {
    mod_number = 2;
  }
  else if (scale_y <= 8)
  {
    mod_number = 4;
  }
  else if (scale_y <= 16)
  {
    mod_number = 8;
  }
  else if (scale_y <= 32)
  {
    mod_number = 16;
  }
  else
  {
    mod_number = (int)(scale_y);
  }
  return (mod_number);
}




// This function gives a yes/no answer to "should we show this label?"
// We search a label hash for a string, if we find a record and it's
// been "found" recently (per mod_number), skip it.
// returns 1 if this is a new label (not found in hash)
// increments skip_label if we determine we should skip
//
// We do it this way because the caller actually already has a skip_label
// variable that might already be 1, and we don't want to clobber
// it with a 0.
//
// The problem with this method is that we might get strings "written"
// at the extreme top or right edge of the display, which means the
// strings wouldn't be visible, but Xastir thinks that it wrote the
// string out visibly.  To partially counteract this I've set it up to
// write only some of the identical strings.  This still doesn't help
// in the cases where a street only comes in from the top or right and
// doesn't have an intersection with another street (and therefore
// another label) within the view.
int check_label_skip(label_string **label_hash, const char *label_text,
                     int mod_number, int *skip_label)

{
  uint8_t hash_index = 0;
  label_string *ptr2 = NULL;
  int new_label = 1;

  // Hash index is just the first
  // character.  Tried using lower 6 bits
  // of first two chars and lower 7 bits
  // of first two chars but the result was
  // slower than just using the first
  // character.
  hash_index = (uint8_t)(label_text[0]);

  ptr2 = label_hash[hash_index];
  while (ptr2 != NULL)     // Step through the list
  {
    // Check 2nd character (fast!)
    if ( (uint8_t)(ptr2->label[1]) == (uint8_t)(label_text[1]) )
    {
      if (strcasecmp(ptr2->label,label_text) == 0)      // Found a match
      {
        new_label = 0;
        ptr2->found = ptr2->found + 1;  // Increment the "found" quantity

        // We change this "mod" number based on zoom level, so that
        // long strings don't overwrite each other, and so that we
        // don't get too many or too few labels drawn.  This will
        // cause us to skip intersections (the tiger files appear to
        // have a label at each intersection).  Between rural and
        // urban areas, this method might not work well.  Urban areas
        // have few intersections, so we'll get fewer labels drawn.  A
        // better method might be to check the screen location for
        // each one and only write the strings if they are far enough
        // apart, and only count a string as written if the start of
        // it is onscreen and the angle is correct for it to be
        // written on the screen.

        // Draw a number of labels appropriate for the zoom level.
        // Labeling: Skip label logic
        if ( ((ptr2->found - 1) % mod_number) != 0)
        {
          (*skip_label)++;
        }
        ptr2 = NULL; // End the loop
      }
      else
      {
        ptr2 = ptr2->next;
      }
    }
    else
    {
      ptr2 = ptr2->next;
    }
  }
  return (new_label);
}




// Compute the rotation angle for label text based on two endpoints of a line
float get_label_angle(int x0, int x1, int y0, int y1)
{
  float diff_X = (int)x1 - x0;
  float diff_Y = (int)y1 - y0;
  float angle = 0.0;  // Angle for the beginning of this polyline

  if (diff_X == 0.0)    // Avoid divide by zero errors
  {
    diff_X = 0.0000001;
  }
  angle = atan( diff_X / diff_Y );    // Compute in radians
  // Convert to degrees
  angle = angle / (2.0 * M_PI );
  angle = angle * 360.0;

  // Change to fit our rotate label function's idea of angle
  angle = 360.0 - angle;


  if ( angle > 90.0 )
  {
    angle += 180.0;
  }
  if ( angle >= 360.0 )
  {
    angle -= 360.0;
  }

  return (angle);
}




// we keep a hash of labels we've found and how many shapes of this
// shapefile have been drawn since the same label was last found.  When
// we encounter a label we haven't found before, we add it to the hash.
void add_label_to_label_hash(label_string **label_hash, const char *label_text)
{
  uint8_t hash_index = 0;
  label_string *ptr2 = NULL;

  // Create a new record for this string
  // and add it to the head of the list.
  // Make sure to "free" this linked
  // list.

  ptr2 = (label_string *)malloc(sizeof(label_string));
  CHECKMALLOC(ptr2);

  memcpy(ptr2->label, label_text, sizeof(ptr2->label));
  ptr2->label[sizeof(ptr2->label)-1] = '\0';  // Terminate string
  ptr2->found = 1;

  // We use first character of string
  // as our hash index.
  hash_index = label_text[0];

  ptr2->next = label_hash[hash_index];
  label_hash[hash_index] = ptr2;
}




// When we're doing SHPT_ARC we wind up switching back and forth between
// line color and label color.  Consolidate that in one spot.
void set_shpt_arc_attributes(Widget w, int color, int lanes, int pattern)
{
  (void)XSetForeground(XtDisplay(w), gc, colors[color]);
  (void)XSetLineAttributes(XtDisplay (w), gc,
                           (lanes)?lanes:1,
                           pattern,
                           CapButt,JoinMiter);
}




// Set the fill style and stipple pattern used for filled polygons
void set_shpt_polygon_fill_stipple(Widget w, int fill_style, int fill_stipple,
                               int draw_filled)
{
  (void)XSetFillStyle(XtDisplay(w), gc, fill_style);

  if (draw_filled != 0 && fill_style == FillStippled)
  {


    switch (fill_stipple)
    {
      case 0:
        (void)XSetStipple(XtDisplay(w), gc,
                          pixmap_13pct_stipple);
        break;
      case 1:
        (void)XSetStipple(XtDisplay(w), gc,
                          pixmap_25pct_stipple);
        break;
      default:
        (void)XSetStipple(XtDisplay(w), gc,
                          pixmap_25pct_stipple);
        break;
    }
  }

}

// Run through all the polygons in the object, return 1 if there are
// any rings that are "holes" (rings running counterclockwise).
// Fill polygon_hole_storage with 1 if the ring is clockwise (not a hole)
// or -1 if it's counterclockwise (a hole)
//
// This code was preceded by the following comment when it still lived
// in draw_shapefile_map (TL;DR:  We are using option 7 below):
//
// --------------------------
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
// Shapefiles also allow identical points to be next to each other
// in the vertice list.  We should look for that and get rid of
// duplicate vertices.
//
// Unfortunately, for Polygon shapes we must make one pass through
// the entire set of rings to see if we have any "hole" rings (as
// opposed to "fill" rings).  If we have any "hole" rings, we must
// handle the drawing of the Shape quite differently.
//
// Speedup:  Only loop through the vertices once, by determining
// hole/fill polygons as we go.
// --------------------------
int preprocess_shp_polygon_holes(SHPObject *object, int *polygon_hole_storage)
{
  int ring;
  int polygon_hole_flag = 0;

  // Run through the entire shape (all rings of it) once.  Create an
  // array of flags that specify whether each ring is a fill or a
  // hole.  If any holes found, set the global polygon_hole_flag as
  // well.

  for (ring = 0; ring < object->nParts; ring++ )
  {

    // Testing for fill or hole ring.  This will
    // determine how we ultimately draw the
    // entire shape.
    //
    switch ( shape_ring_direction( object, ring) )
    {
      case  0:    // Error in trying to compute whether fill or hole
        fprintf(stderr,"Error in computing fill/hole ring\n");
        /* Falls through. */
      case  1:
        // It's a fill ring.  Do nothing for these two cases except
        // clear the flag in our storage
        polygon_hole_storage[ring] = 0;
        break;
      case -1:
        // It's a hole ring Add it to our list of hole rings here and
        // set a flag.  That way we won't have to run through
        // SHPRingDir_2d again in the next loop.
        polygon_hole_flag++;
        polygon_hole_storage[ring] = 1;
        break;
    }
  }
  return (polygon_hole_flag);
}




// This function takes a SHPT_POLYGON object and a vector of ints of length
// equal to the number of parts in the polygon and returns a graphics context
// with a clipping region set to mask out the holes.
//
// high_water_mark_index is used for debugging and marks the maximum number
// of points we've ever encountered in a shape.
GC get_hole_clipping_context(Widget w, SHPObject *object,
                             int *polygon_hole_storage,
                             int *high_water_mark_index)
{
  XRectangle rectangle;
  long width, height;
  double top_ll, left_ll, bottom_ll, right_ll;
  Region region[3];
  int temp_region1;
  int temp_region2;
  int temp_region3;
  long x,y;
  int ok;
  int ring;
  int index;
  int i;
  XPoint points[MAX_MAP_POINTS];
  GC gc_temp = NULL;
  XGCValues gc_temp_values;

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


  // Create three regions and rotate between them, due to the
  // XSubtractRegion() needing three parameters.  If we later find
  // that two of the parameters can be repeated, we can simplify our
  // code.  We'll rotate through them mod 3.

  temp_region1 = 0;

  // Create empty region
  region[temp_region1] = XCreateRegion();

  // Draw a rectangular clip-mask inside the Region.  Use the same
  // extents as the full Shape.

  // Set up the real sizes from the Shape
  // extents.
  top_ll    = object->dfYMax;
  left_ll   = object->dfXMin;
  bottom_ll = object->dfYMin;
  right_ll  = object->dfXMax;

  // Convert point to screen coordinates:
  ok = convert_ll_to_screen_coords(&x, &y, left_ll, top_ll);

  if (ok)
  {
    // Here we check for really wacko points that will cause problems
    // with the X drawing routines, and fix them.
    (void) clip_x_y_pair(&x, &y, 0l, screen_width, 0l, screen_height);
  }

  // Convert point to screen coordinates
  ok = convert_ll_to_screen_coords(&width, &height,
                                   right_ll, bottom_ll);
  if (ok)
  {
    // Here we check for really wacko points that will cause problems
    // with the X drawing routines, and fix them.
    (void) clip_x_y_pair(&width, &height, 1l, screen_width, 1l, screen_height);
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

  // Create the initial region containing a filled rectangle.
  XUnionRectWithRegion(&rectangle,
                       region[temp_region1],
                       region[temp_region1]);

  // Create a region for each set of hole vertices (CCW rotation of
  // the vertices) and subtract each from the rectangle region.
  for (ring = 0; ring < object->nParts; ring++ )
  {
    int endpoint;
    int on_screen;


    if (polygon_hole_storage[ring] == 1)
    {
      // It's a hole polygon.  Cut the
      // hole out of our rectangle region.
      int num_vertices = 0;

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
      {
        endpoint = object->panPartStart[ring+1];
      }
      //else endpoint = object->nVertices;
      else
      {
        endpoint = object->panPartStart[0] + object->nVertices;
      }

      i = 0;  // i = Number of points to draw for one ring
      on_screen = 0;

      // index = ptr into the shapefile's array of points
      // i = the index into our XPoint array
      for (index = object->panPartStart[ring]; index < endpoint; index++)
      {

        ok = get_vertex_screen_coords(object, index, &x,&y);
        if (ok)
        {
          // Here we check for really wacko points that will
          // cause problems with the X drawing routines, and
          // fix them.  Increment on_screen if any of the
          // points might be on screen.
          on_screen += clip_x_y_pair(&x, &y, 0l, screen_width, 0l, screen_height);

          points[i].x = l16(x);
          points[i].y = l16(y);

          i++;
        }

        if (i > *high_water_mark_index)
        {
          *high_water_mark_index = i;
        }

      }   // End of converting vertices for a ring


      // Create and subtract the region only if it might be on screen.
      if (on_screen)
      {
        temp_region2 = (temp_region1 + 1) % 3;
        temp_region3 = (temp_region1 + 2) % 3;

        // Create empty regions.
        region[temp_region2] = XCreateRegion();
        region[temp_region3] = XCreateRegion();

        // Draw the hole polygon
        if (num_vertices >= 3)
        {
          XDestroyRegion(region[temp_region2]); // Release the old
          region[temp_region2] = XPolygonRegion(points,
                                                num_vertices,
                                                WindingRule);
        }
        else
        {
          fprintf(stderr,
                  "draw_shapefile_map:XPolygonRegion with too few vertices:%d\n",
                  num_vertices);
        }

        // Subtract region2 from region1 and put the result into
        // region3.
        XSubtractRegion(region[temp_region1], region[temp_region2],
                        region[temp_region3]);

        // Get rid of the two regions we no longer need
        XDestroyRegion(region[temp_region1]);
        XDestroyRegion(region[temp_region2]);

        // Indicate the final result region for the next iteration or
        // the exit of the loop.
        temp_region1 = temp_region3;
      }
    }
  }

  // region[temp_region1] now contains a clip-mask of the original
  // polygon with holes cut out of it (swiss-cheese rectangle).

  // Create temporary GC.  It looks like we don't need this to create
  // the regions, but we'll need it when we draw the filled polygons
  // onto the map pixmap using the final region as a clip-mask.

  gc_temp = XCreateGC(XtDisplay(w),
                      XtWindow(w),
                      0,
                      &gc_temp_values);
  // now copy the fill style and stipple from gc.
  XCopyGC(XtDisplay(w),
          gc,
          (GCFillStyle|GCStipple),
          gc_temp);

  // Set the clip-mask into the GC.  This GC
  // is now ruined for other purposes, so
  // destroy it when we're done drawing this
  // one shape.
  XSetRegion(XtDisplay(w), gc_temp, region[temp_region1]);
  XDestroyRegion(region[temp_region1]);

  return (gc_temp);
}



// This function clips an x/y pair to the bounding rectangle specified
// by the min/max values passed in.
//
// it returns 1 if we didn't clip and 0 if we did.
int clip_x_y_pair(long *x, long *y, long x_min, long x_max, long y_min, long y_max)
{
  int on_screen=0;
  // Here we check for really wacko points that will
  // cause problems with the X drawing routines, and
  // fix them.  Increment on_screen if any of the
  // points might be on screen.
  if (*x >  x_max)
  {
    *x =  x_max;
  }
  else if (*x < x_min)
  {
    *x = x_min;
  }
  else
  {
    on_screen++;
  }

  if (*y >  y_max)
  {
    *y =  y_max;
  }
  else if (*y < y_min)
  {
    *y = y_min;
  }
  else
  {
    on_screen++;
  }

  return (on_screen);
}




// draw an unfilled polygon with dashed boundary in given color.
void draw_polygon_boundary_dashed(Widget w, int color, XPoint *points,
                                  int numPoints)
{
  (void)XSetForeground(XtDisplay(w), gc, colors[color]);
  (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineOnOffDash, CapButt,
                            JoinMiter);
  (void)XDrawLines(XtDisplay(w), pixmap, gc, points, l16(numPoints),
                   CoordModeOrigin);

  // reset back to solid
  (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineSolid,
                            CapButt, JoinMiter);
}




// draw a (possibly) filled polygon using the specified graphics
// context into the specified pixmap out of XPoint array of size
// numPoints, in specified boundary and fill colors and with specified
// line width and pattern.
//
//
// The graphics context passed in is either the normal one or the one
// that does polygon hole clipping.
//
// "do_the_fill" determines whether we should do the fill or not.
//
void draw_filled_polygon(Widget w, GC theGC, XPoint *points, int numPoints,
                         int color, int fill_color, int lanes, int pattern,
                         int do_the_fill)
{
  (void)XSetLineAttributes(XtDisplay(w), theGC, (lanes)?lanes:1,
                           pattern, CapButt, JoinMiter);
  (void)XSetForeground(XtDisplay(w), gc, colors[color]);
  if (do_the_fill)
  {
    (void)XSetForeground(XtDisplay(w), theGC, colors[fill_color]);
    if (numPoints >3)
    {
      (void)XFillPolygon(XtDisplay(w), pixmap, theGC, points, numPoints,
                         Nonconvex, CoordModeOrigin);
    }
    else
    {
      fprintf(stderr,"draw_filled_polygon: too few points: %d, Skipping XFillPolygon()\n",numPoints);
    }
  }

  // Draw the border
  (void)XSetForeground(XtDisplay(w), gc, colors[color]);
  (void)XSetFillStyle(XtDisplay(w), gc, FillSolid);
  (void)XDrawLines(XtDisplay(w), pixmap, gc, points, l16(numPoints),
                   CoordModeOrigin);
}




// The wx alerts get drawn in a way almost, but not quite, like others,
// but into a different graphics context that already has its attributes
// set.  So we have a second polygon drawing routine.
void draw_wx_polygon(Widget w, XPoint *points, int numPoints)
{
  (void)XSetFillStyle(XtDisplay(w), gc_tint, FillStippled);

  if (numPoints >= 3)
  {
    (void)XFillPolygon(XtDisplay(w), pixmap_alerts, gc_tint, points, numPoints,
                       Nonconvex, CoordModeOrigin);
  }
  else
  {
    fprintf(stderr,
            "draw_wx_polygon:Too few points:%d, Skipping XFillPolygon()",
            numPoints);
  }

  (void)XSetFillStyle(XtDisplay(w), gc_tint, FillSolid);
  (void)XDrawLines(XtDisplay(w), pixmap_alerts, gc_tint,
                   points, l16(numPoints), CoordModeOrigin);
}



// This function selects a point at which to display a shape's label.
//
// If label_method is one and the lat/lon has been set from dbfawk,
// use that point.
//
// otherwise use the center of the shape's bounding box.
void choose_polygon_label_point(SHPObject *object, float *lon, float *lat)
{
  if (label_method == 1 && (label_lat != 0.0 || label_lon != 0.0))
  {
    *lon = label_lon;
    *lat = label_lat;
  }
  else
  {
    *lon = (float) (object->dfXMax + object->dfXMin)/2.0;
    *lat = (float) (object->dfYMax + object->dfYMin)/2.0;
  }
}




// We have a slew of variables that control shapefile rendering.  They are all
// static variables in this file and therefore need to be initialized every
// time draw_shapefile_map is called, lest unset variables in this shapefiles's
// dbfawk file remain at non-default values set by the previously rendered
// shapefile's.
//
// Note that these are intended to match the defaults set in
// dbfawk_default_rules, but because INT_MAX and FONT_DEFAULT can't be
// embedded in in that default rule, the former is hard coded over there,
// and the default font isn't set at all.  Since we set it here, there's no
// problem.
void initialize_rendering_variables(void)
{
  color=8;
  lanes=1;
  filled=0;
  fill_style=0;
  fill_color=13;
  fill_stipple=0;
  pattern=0;
  display_level=INT_MAX;
  min_display_level=0;
  label_level=0;
  label_color=8;
  font_size=FONT_DEFAULT;
  label_method=0;
  label_lon=0.0;
  label_lat=0.0;
}
#endif  // HAVE_LIBSHP


