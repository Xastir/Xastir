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

#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }
#define CHECKREALLOC(m)  if (!m) { fprintf(stderr, "***** Realloc Failed *****\n"); exit(0); }

#ifdef HAVE_LIBSHP
#ifdef HAVE_LIBPCRE
  #include "awk.h"
  #include "dbfawk.h"
#endif /* HAVE_LIBPCRE */
#ifdef HAVE_SHAPEFIL_H
  #include <shapefil.h>
#else   // HAVE_SHAPEFIL_H
  #ifdef HAVE_LIBSHP_SHAPEFIL_H
    #include <libshp/shapefil.h>
  #else   // HAVE_LIBSHP_SHAPEFIL_H
    #error HAVE_LIBSHP defined but no corresponding include defined
  #endif // HAVE_LIBSHP_SHAPEFIL_H
#endif // HAVE_SHAPEFIL_H

extern int npoints;        /* tsk tsk tsk -- globals */

#include <rtree/index.h>
#include "shp_hash.h"

// Must be last include file
#include "leak_detection.h"



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
    //fprintf(stderr,"Hitarray now at %d\n",RTree_hitarray_size);
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
 * Shapefiles, but only from a few sources (NOAA, Mapshots.com, and
 * ESRI/GeographyNetwork.com).  We don't handle some of the more
 * esoteric formats.  We now handle the "hole" drawing in polygon
 * shapefiles, where one direction around the ring means a fill, and
 * the other direction means a hole in the polygon.
 *
 * Note that we must currently hard-code the file-recognition
 * portion and the file-drawing portion, because every new source of
 * Shapefiles has a different format, and the fields and field
 * definitions can all change between them.
 *
 * If alert is NULL, draw every shape that fits the screen.  If
 * non-NULL, draw only the shape that matches the zone number.
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


USGS Quads Overlay (24kgrid.shp) from
http://data.geocomm.com/quadindex/
----------------------------------
field name  type     width,dec  Example
NAME        string     (30,0)   Lummi Bay OE W
STATE       string      (2,0)   WA
LAT         string      (6,0)   48.750
LON         string      (8,0)   -122.750
MRC         string      (8,0)   48122-G7


// Need to figure out which type of alert it is, select the corresponding shapefile,
// then store the shapefile AND the alert_tag in the alert hash .filename list?
// and draw the map.  Add an item to alert hash to keep track?

// The last parameter denotes loading into pixmap_alerts instead of pixmap or pixmap_final
// Here's the old APRS-type map call:
//map_search (w, alert_scan, alert, &alert_count,(int)(alert_status[i + 2] == DATA_VIA_TNC || alert_status[i + 2] == DATA_VIA_LOCAL), DRAW_TO_PIXMAP_ALERTS);

// Check the zone name(s) to see which Shapefile(s) to use.

            switch (zone[4]) {
                case ('C'): // County File (c_16my01.shp)
                    break;
***             case ('A'): // County Warning File (w_24ja01.shp)
                    break;
                case ('Z'): // Zone File (z_16mr01.shp, z_16my01.shp, mz24ja01.shp, oz09de99.shp)
                    break;
                case ('F'): // Fire weather (fz_ddmmyy.shp)
                    break;
***             case ('A'): // Canadian Area (a_mmddyy.shp)
                    break;
                case ('R'): // Canadian Region (r_mmddyy.shp)
                    break;
            }


 **********************************************************/

#ifdef WITH_DBFAWK
static dbfawk_sig_info *Dbf_sigs = NULL;
static awk_symtab *Symtbl = NULL;
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
    "dbfinfo=\"\"; key=\"\"; lanes=1; color=8; fill_color=13; fill_stipple=0; name=\"\"; filled=0; fill_style=0; pattern=0; display_level=65536; label_level=0",
    0,
    0
  },
};
#define dbfawk_default_nrules (sizeof(dbfawk_default_rules)/sizeof(dbfawk_default_rules[0]))
static dbfawk_sig_info *dbfawk_default_sig = NULL;
#endif  // WITH_DBFAWK

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
#ifndef WITH_DBFAWK
  char            ftype[15];
  int             nWidth, nDecimals;
#endif /*!WITH_DBFAWK*/
  SHPHandle       hSHP;
  int             nShapeType, nEntities;
  double          adfBndsMin[4], adfBndsMax[4];
  char            *sType;
  unsigned long   my_lat, my_long;
  long            x,y;
  int             ok, index;
  int             polygon_hole_flag;
  int             *polygon_hole_storage;
  GC              gc_temp = NULL;
  XGCValues       gc_temp_values;
  Region          region[3];
  int             temp_region1;
  int             temp_region2;
  int             temp_region3;
  int             gps_flag = 0;
  char            gps_label[100];
  int             gps_color = 0x0c;
#ifndef WITH_DBFAWK
  int             road_flag = 0;
  int             lake_flag = 0;
  int             river_flag = 0;
  int             railroad_flag = 0;
  int             school_flag = 0;
  int             path_flag = 0;
  int             city_flag = 0;
#endif /*!WITH_DBFAWK*/
  int             quad_overlay_flag = 0;
#ifndef WITH_DBFAWK
  int             mapshots_labels_flag = 0;
#endif /*!WITH_DBFAWK*/
  int             weather_alert_flag = 0;
  char            *filename;  // filename itself w/o directory
#ifndef WITH_DBFAWK
  int             search_field1 = 0;
  int             search_field2 = -1;
  char            search_param1[10];
  char            search_param2[10];
#endif /* !WITH_DBFAWK */
  int             found_shape = -1;
  int             ok_to_draw = 0;
  int             high_water_mark_i = 0;
  int             high_water_mark_index = 0;
  char            quad_label[100];
  char            status_text[MAX_FILENAME];
#ifdef WITH_DBFAWK
  /* these have to be static since I recycle Symtbl between calls */
  static char     dbfsig[1024],dbffields[1024],name[64],key[64],sym[4];
  static int      color,lanes,filled,pattern,display_level,label_level;
  static int      fill_style,fill_color;
  static int      fill_stipple;
  //static int layer;
  dbfawk_sig_info *sig_info = NULL;
  dbfawk_field_info *fld_info = NULL;


  int draw_filled_orig;
#endif  // WITH_DBFAWK
  int draw_filled;
  static int label_color = 8; /* set by dbfawk.  Otherwise it's black. */
  static int font_size = FONT_DEFAULT; // set by dbfawk, else this default

  typedef struct _label_string
  {
    char   label[50];
    int    found;
    struct _label_string *next;
  } label_string;

  // Define hash table for label pointers
  label_string *label_hash[256];
  // And the index into it
  uint8_t hash_index = 0;

  label_string *ptr2 = NULL;

  struct Rect viewportRect;
  double rXmin, rYmin, rXmax,rYmax;
  shpinfo *si;
  int nhits;

  // pull this out of the map_draw_flags
  draw_filled = mdf->draw_filled;

  // Initialize the hash table label pointers
  for (i = 0; i < 256; i++)
  {
    label_hash[i] = NULL;
  }


#ifdef WITH_DBFAWK
  // We're going to change draw_filled a bunch if we've got Auto turned
  // on, but we have to check --- save this!
  draw_filled_orig=draw_filled;

  // Re-initialize these static variables every time through here.
  // Otherwise, if a dbfawk file forgets to set one, we'd use what the
  // last map used.  Sometimes that's ugly.
  color=8;
  lanes=1;
  filled=0;
  fill_style=0;
  fill_color=13;
  fill_stipple=0;
  pattern=0;
  display_level=8192;
  label_level=0;
  label_color=8;
  font_size=FONT_DEFAULT;
#endif  // WITH_DBFAWK

#ifdef WITH_DBFAWK
  if (Dbf_sigs == NULL)
  {
    Dbf_sigs = dbfawk_load_sigs(get_data_base_dir("config"),".dbfawk");
  }

  if (debug_level & 16)
    fprintf(stderr,"DBFAWK signatures %sfound in %s.\n",
            (Dbf_sigs)?" ":"NOT ",get_data_base_dir("config"));

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
#endif  // WITH_DBFAWK

  //fprintf(stderr,"*** Alert color: %d ***\n",alert_color);

  // We don't draw the shapes if alert_color == -1
  if (alert_color != 0xff)
  {
    ok_to_draw++;
  }

#ifndef WITH_DBFAWK
  search_param1[0] = '\0';
  search_param2[0] = '\0';
#endif /* !WITH_DBFAWK */

  xastir_snprintf(file, sizeof(file), "%s/%s", dir, filenm);

  // Create a shorter filename for display (one that fits the
  // status line more closely).  Subtract the length of the
  // "Indexing " and/or "Loading " strings as well.
  if (strlen(filenm) > (41 - 9))
  {
    int avail = 41 - 11;
    int new_len = strlen(filenm) - avail;

    xastir_snprintf(short_filenm,
                    sizeof(short_filenm),
                    "..%s",
                    &filenm[new_len]);
  }
  else
  {
    xastir_snprintf(short_filenm,
                    sizeof(short_filenm),
                    "%s",
                    filenm);
  }

  //fprintf(stderr,"draw_shapefile_map:start:%s\n",file);

  filename = filenm;
  i = strlen(filenm);
  while ( (i >= 0) && (filenm[i] != '/') )
  {
    filename = &filenm[i--];
  }
  //fprintf(stderr,"draw_shapefile_map:filename:%s\ttitle:%s\n",filename,alert->title);

  if (alert)
  {
    weather_alert_flag++;
  }

  // Check for ~/.xastir/tracklogs directory.  We set up the
  // labels and colors differently for these types of files.
//    if (strstr(filenm,".xastir/tracklogs")) { // We're in the ~/.xastir/tracklogs directory
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

#ifdef WITH_DBFAWK
  *dbfsig = '\0';
  fieldcount = dbfawk_sig(hDBF,dbfsig,sizeof(dbfsig));
#else   // WITH_DBFAWK
  fieldcount = DBFGetFieldCount(hDBF);
#endif /* !WITH_DBFAWK */
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
#ifdef WITH_DBFAWK
    fprintf(stderr,"DBF signature: %s\n",dbfsig);
#endif  // WITH_DBFAWK
  }
#ifdef WITH_DBFAWK
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
    if (sig_info)           /* we've got a .dbfawk, so set up symtbl */
    {

      if (!Symtbl)
      {
        Symtbl = awk_new_symtab();
        awk_declare_sym(Symtbl,"dbffields",STRING,dbffields,sizeof(dbffields));
        awk_declare_sym(Symtbl,"color",INT,&color,sizeof(color));
        awk_declare_sym(Symtbl,"lanes",INT,&lanes,sizeof(lanes));
        //awk_declare_sym(Symtbl,"layer",INT,&layer,sizeof(layer));
        awk_declare_sym(Symtbl,"name",STRING,name,sizeof(name));
        awk_declare_sym(Symtbl,"key",STRING,key,sizeof(key));
        awk_declare_sym(Symtbl,"symbol",STRING,sym,sizeof(sym));
        awk_declare_sym(Symtbl,"filled",INT,&filled,sizeof(filled));
        awk_declare_sym(Symtbl,"fill_style",INT,&fill_style,sizeof(fill_style));
        awk_declare_sym(Symtbl,"fill_color",INT,&fill_color,sizeof(fill_color));
        awk_declare_sym(Symtbl,"fill_stipple",INT,&fill_stipple,sizeof(fill_stipple));
        awk_declare_sym(Symtbl,"pattern",INT,&pattern,sizeof(pattern));
        awk_declare_sym(Symtbl,"display_level",INT,&display_level,sizeof(display_level));
        awk_declare_sym(Symtbl,"label_level",INT,&label_level,sizeof(label_level));
        awk_declare_sym(Symtbl,"label_color",INT,&label_color,sizeof(label_color));
        awk_declare_sym(Symtbl,"font_size",INT,&font_size,sizeof(font_size));
      }
      if (awk_compile_program(Symtbl,sig_info->prog) < 0)
      {
        fprintf(stderr,"Unable to compile .dbfawk program\n");

        if (sig_info != NULL && sig_info != dbfawk_default_sig  && (sig_info->sig == NULL))
        {
          dbfawk_free_sigs(sig_info);
        }
        return;
      }
      awk_exec_begin(sig_info->prog); /* execute a BEGIN rule if any */

      /* find out which dbf fields we care to read */
      fld_info = dbfawk_field_list(hDBF, dbffields);

    }
    else                    /* should never be reached anymore! */
    {
      fprintf(stderr,"No DBFAWK signature for %s and no default!\n",filenm);
      //exit(1);  // Debug
      return;
    }
  }
#endif /* WITH_DBFAWK */
  /*
   * DBFAWK: all this WX junk following to set up the search fields and
   * parameters is now done by the dbfawk file which sets the "key"
   * variable to the appropriate search key for each record which is
   * compared to the alert->title[].
   */
#ifndef WITH_DBFAWK
  // If we're doing weather alerts and index is not filled in yet
  if (weather_alert_flag && (alert->index == -1) )
  {

    // For weather alerts:
    // Need to figure out from the initial characters of the filename which
    // type of file we're using, then compute the fields we're looking for.
    // After we know that, need to look in the DBF file for a match.  Once
    // we find a match, we can open up the SHX/SHP files, go straight to
    // the shape we want, and draw it.
    switch (filenm[0])
    {

      case 'c':   // County File
        // County, c_ files:  WI_C037
        // STATE  CWA  COUNTYNAME
        // AL     BMX  Morgan
        // Need fields 0/1:
        search_field1 = 0;  // STATE
        search_field2 = 3;  // FIPS
        xastir_snprintf(search_param1,sizeof(search_param1),"%c%c",
                        alert->title[0],
                        alert->title[1]);
        xastir_snprintf(search_param2,sizeof(search_param2),"%c%c%c",
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
        xastir_snprintf(search_param1,sizeof(search_param1),"%c%c%c",
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
        xastir_snprintf(search_param1,sizeof(search_param1),"%c%c%c%c%c%c",
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
        xastir_snprintf(search_param1,sizeof(search_param1),"%c%c%c%c%c%c",
                        alert->title[0],
                        alert->title[1],
                        alert->title[3],
                        alert->title[4],
                        alert->title[5],
                        alert->title[6]);
        break;

      case 'z':   // Zone File
      case 'f':   // Fire zone file, KS_F033
      default:
        // Weather alert zones, z_ files:  KS_Z033
        // STATE_ZONE
        // AK225
        // Need field 4
        search_field1 = 4;  // STATE_ZONE
        search_field2 = -1;
        xastir_snprintf(search_param1,sizeof(search_param1),"%c%c%c%c%c",
                        alert->title[0],
                        alert->title[1],
                        alert->title[4],
                        alert->title[5],
                        alert->title[6]);
        break;
    }

    //fprintf(stderr,"Search_param1: %s,\t",search_param1);
    //fprintf(stderr,"Search_param2: %s\n",search_param2);
  } /* weather_alert */

  for (i=0; i < fieldcount; i++)
  {
    char szTitle[12];

    switch (DBFGetFieldInfo(hDBF, i, szTitle, &nWidth, &nDecimals))
    {
      case FTString:
        xastir_snprintf(ftype,sizeof(ftype),"string");
        break;

      case FTInteger:
        xastir_snprintf(ftype,sizeof(ftype),"integer");
        break;

      case FTDouble:
        xastir_snprintf(ftype,sizeof(ftype),"float");
        break;

      case FTInvalid:
        xastir_snprintf(ftype,sizeof(ftype),"invalid/unsupported");
        break;

      default:
        xastir_snprintf(ftype,sizeof(ftype),"unknown");
        break;
    }

    // Check for quad overlay type of map
    if (strstr(filename,"24kgrid"))    // USGS Quad overlay file
    {
      quad_overlay_flag++;
    }

    // Check the filename for mapshots.com filetypes to see what
    // type of file we may be dealing with.
    if (strncasecmp(filename,"tgr",3) == 0)     // Found Mapshots or GeographyNetwork file
    {

      if (strstr(filename,"lpt"))             // Point file
      {
        mapshots_labels_flag++;
        if (debug_level & 16)
        {
          fprintf(stderr,"*** Found point file ***\n");
          break;
        }
        else
        {
          break;
        }
      }
      else if (strstr(filename,"plc"))           // Designated Places:  Arlington
      {
        city_flag++;
        mapshots_labels_flag++;
        if (debug_level & 16)
        {
          fprintf(stderr,"*** Found (Designated Places) ***\n");
          break;
        }
        else
        {
          break;
        }
      }
      else if (strstr(filename,"ctycu"))      // County Boundaries: WA, Snohomish
      {
        if (debug_level & 16)
        {
          fprintf(stderr,"*** Found county (mapshots county) ***\n");
          break;
        }
        else
        {
          break;
        }
      }
      else if (strstr(filename,"lkA"))        // Roads
      {
        road_flag++;
        mapshots_labels_flag++;
        if (debug_level & 16)
        {
          fprintf(stderr,"*** Found some roads (mapshots roads) ***\n");
          break;
        }
        else
        {
          break;
        }
      }
      else if (strstr(filename,"lkB"))        // Railroads
      {
        railroad_flag++;
        mapshots_labels_flag++;
        if (debug_level & 16)
        {
          fprintf(stderr,"*** Found some railroads (mapshots railroads) ***\n");
          break;
        }
        else
        {
          break;
        }
      }
      else if (strstr(filename,"lkC"))        // Paths/etc.  Pipelines?  Transmission lines?
      {
        path_flag++;
        if (debug_level & 16)
        {
          fprintf(stderr,"*** Found some paths (mapshots paths/etc) ***\n");
          break;
        }
        else
        {
          break;
        }
      }
      else if (strstr(filename,"lkH"))        // Rivers/Streams/Lakes/Glaciers
      {
        river_flag++;
        mapshots_labels_flag++;
        if (debug_level & 16)
        {
          fprintf(stderr,"*** Found water (mapshots rivers/streams/lakes/glaciers) ***\n");
          break;
        }
        else
        {
          break;
        }
      }
      else if (  strstr(filename,"elm")       // Elementary school district
                 || strstr(filename,"mid")       // Middle school district
                 || strstr(filename,"sec")       // Secondary school district
                 || strstr(filename,"uni") )     // Unified school district
      {
        school_flag++;
        if (debug_level & 16)
        {
          fprintf(stderr,"*** Found school district ***\n");
          break;
        }
        else
        {
          break;
        }
      }
      else if (strstr(filename,"urb"))        // Urban areas: Seattle, WA
      {
        if (debug_level & 16)
        {
          fprintf(stderr,"*** Found (mapshots urban areas) ***\n");
          break;
        }
        else
        {
          break;
        }
      }
      else if (strstr(filename,"wat"))        // Bodies of water, creeks/lakes/glaciers
      {
        lake_flag++;
        mapshots_labels_flag++;
        if (debug_level & 16)
        {
          fprintf(stderr,"*** Found some water (mapshots bodies of water, creeks/lakes/glaciers) ***\n");
          break;
        }
        else
        {
          break;
        }
      }
    }


    // Attempt to guess which type of shapefile we're dealing
    // with, and how we should draw it.
    // If debug is on, we want to print out every field, otherwise
    // break once we've made our guess on the type of shapefile.
    if (debug_level & 16)
    {
      fprintf(stderr,"%15.15s\t%15s  (%d,%d)\n", szTitle, ftype, nWidth, nDecimals);
    }

    if (strncasecmp(szTitle, "SPEEDLIM", 8) == 0)
    {
      // sewroads shapefile?
      road_flag++;
      if (debug_level & 16)
      {
        fprintf(stderr,"*** Found some roads (SPEEDLIM*) ***\n");
      }
      else
      {
        break;
      }
    }
    else if (strncasecmp(szTitle, "US_RIVS_ID", 10) == 0)
    {
      // which shapefile?
      river_flag++;
      if (debug_level & 16)
      {
        fprintf(stderr,"*** Found some rivers (US_RIVS_ID*) ***\n");
      }
      else
      {
        break;
      }
    }
    else if (strcasecmp(szTitle, "FEATURE") == 0)
    {
      char *attr_str;
      int j;
      for (j=0; j < recordcount; j++)
      {
        if (fieldcount >= (i+1))
        {
          attr_str = (char*)DBFReadStringAttribute(hDBF, j, i);
          if (strncasecmp(attr_str, "LAKE", 4) == 0)
          {
            // NOAA Lakes and Water Bodies (lk17de98) shapefile
            lake_flag++;
            if (debug_level & 16)
            {
              fprintf(stderr,"*** Found some lakes (FEATURE == LAKE*) ***\n");
            }
            break;
          }
          else if (strstr(attr_str, "Highway") != NULL ||
                   strstr(attr_str, "highway") != NULL ||
                   strstr(attr_str, "HIGHWAY") != NULL)
          {
            // NOAA Interstate Highways of the US (in011502) shapefile
            // NOAA Major Roads of the US (rd011802) shapefile
            road_flag++;
            if (debug_level & 16)
            {
              fprintf(stderr,"*** Found some roads (FEATURE == *HIGHWAY*) ***\n");
            }
            break;
          }
        }
      }
      if (!(debug_level & 16) && (lake_flag || road_flag))
      {
        break;
      }
    }
    else if (strcasecmp(szTitle, "LENGTH") == 0 ||
             strcasecmp(szTitle, "RR")     == 0 ||
             strcasecmp(szTitle, "HUC")    == 0 ||
             strcasecmp(szTitle, "TYPE")   == 0 ||
             strcasecmp(szTitle, "SEGL")   == 0 ||
             strcasecmp(szTitle, "PMILE")  == 0 ||
             strcasecmp(szTitle, "ARBSUM") == 0 ||
             strcasecmp(szTitle, "PNAME")  == 0 ||
             strcasecmp(szTitle, "OWNAME") == 0 ||
             strcasecmp(szTitle, "PNMCD")  == 0 ||
             strcasecmp(szTitle, "OWNMCD") == 0 ||
             strcasecmp(szTitle, "DSRR")   == 0 ||
             strcasecmp(szTitle, "DSHUC")  == 0 ||
             strcasecmp(szTitle, "USDIR")  == 0)
    {
      // NOAA Rivers of the US (rv14fe02) shapefile
      // NOAA Rivers of the US Subset (rt14fe02) shapefile
      river_flag++;
      if (river_flag >= 14)
      {
        if (debug_level & 16)
        {
          fprintf(stderr,"*** Found some rivers (NOAA Rivers of the US or Subset) ***\n");
        }
        else
        {
          break;
        }
      }
    }
  } /* ... end for (i = 0; i < fieldcount; i++) */
#endif /* !WITH_DBFAWK */

  // Search for specific record if we're doing alerts
  if (weather_alert_flag && (alert->index == -1) )
  {
    int done = 0;
#ifndef WITH_DBFAWK
    char *string1;
    char *string2;
#endif /* !DBFAWK */

    // Step through all records
    for( i = 0; i < recordcount && !done; i++ )
    {
#ifdef WITH_DBFAWK
      int keylen;

      if (sig_info)
      {
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
#else /* !WITH_DBFAWK */
      char *ptr;
      switch (filenm[0])
      {
        case 'c':   // County File
          // Remember that there's only one place for
          // internal storage of the DBF string.  This is
          // why this code is organized with two "if"
          // statements.
          if (fieldcount >= (search_field1 + 1) )
          {
            string1 = (char *)DBFReadStringAttribute(hDBF,i,search_field1);
            if (!strncasecmp(search_param1,string1,2))
            {
              //fprintf(stderr,"Found state\n");
              if (fieldcount >= (search_field2 + 1) )
              {
                string2 = (char *)DBFReadStringAttribute(hDBF,i,search_field2);
                ptr = string2;
                ptr += 2;   // Skip past first two characters of FIPS code
                if (!strncasecmp(search_param2,ptr,3))
                {
//fprintf(stderr,"Found it!  %s\tShape: %d\n",string1,i);
                  done++;
                  found_shape = i;
                }
              }
            }
          }
          break;
        case 'w':   // County Warning Area File
          if (fieldcount >= (search_field1 + 1) )
          {
            string1 = (char *)DBFReadStringAttribute(hDBF,i,search_field1);
            if ( !strncasecmp(search_param1,string1,strlen(string1))
                 && (strlen(string1) != 0) )
            {
//fprintf(stderr,"Found it!  %s\tShape: %d\n",string1,i);
              done++;
              found_shape = i;
            }
          }
          break;
        case 'o':   // Offshore Marine Area File
          if (fieldcount >= (search_field1 + 1) )
          {
            string1 = (char *)DBFReadStringAttribute(hDBF,i,search_field1);
            if ( !strncasecmp(search_param1,string1,strlen(string1))
                 && (strlen(string1) != 0) )
            {
//fprintf(stderr,"Found it!  %s\tShape: %d\n",string1,i);
              done++;
              found_shape = i;
            }
          }
          break;
        case 'm':   // Marine Area File
          if (fieldcount >= (search_field1 + 1) )
          {
            string1 = (char *)DBFReadStringAttribute(hDBF,i,search_field1);
            if ( !strncasecmp(search_param1,string1,strlen(string1))
                 && (strlen(string1) != 0) )
            {
//fprintf(stderr,"Found it!  %s\tShape: %d\n",string1,i);
              done++;
              found_shape = i;
            }
          }
          break;
        case 'z':   // Zone File
        case 'f':   // Fire zone file
          if (fieldcount >= (search_field1 + 1) )
          {
            string1 = (char *)DBFReadStringAttribute(hDBF,i,search_field1);
            if ( !strncasecmp(search_param1,string1,strlen(string1))
                 && (strlen(string1) != 0) )
            {
//fprintf(stderr,"Found it!  %s\tShape: %d\n",string1,i);
              done++;
              found_shape = i;
            }
          }
        default:
          break;
      }
#endif /* !WITH_DBFAWK */
    }
    alert->index = found_shape; // Fill it in 'cuz we just found it
  } /* if (weather_alert_flag && alert_index == -1)... */
  else if (weather_alert_flag)
  {
    // We've been here before and we already know the index into the
    // file to fetch this particular shape.
    found_shape = alert->index;
    if (debug_level & 16)
    {
      fprintf(stderr,"wx_alert: found_shape = %d\n",found_shape);
    }
  }

  //fprintf(stderr,"Found shape: %d\n", found_shape);

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

#ifdef WITH_DBFAWK
    dbfawk_free_info(fld_info);
    if (sig_info != NULL && sig_info != dbfawk_default_sig  && (sig_info->sig == NULL))
    {
      dbfawk_free_sigs(sig_info);
    }
#endif  // WITH_DBFAWK

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

#ifdef WITH_DBFAWK
    dbfawk_free_info(fld_info);
    if (sig_info != NULL && sig_info != dbfawk_default_sig  && (sig_info->sig == NULL))
    {
      dbfawk_free_sigs(sig_info);
    }
#endif  // WITH_DBFAWK

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
  get_viewport_lat_lon(&rXmin, &rYmin, &rXmax, &rYmax);
  viewportRect.boundary[0] = (RectReal) rXmin;
  viewportRect.boundary[1] = (RectReal) rYmin;
  viewportRect.boundary[2] = (RectReal) rXmax;
  viewportRect.boundary[3] = (RectReal) rYmax;

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
      fprintf(stderr,"Multi-Point Shapefile format not implemented: %s\n",file);
      sType = "MultiPoint";
      DBFClose( hDBF );   // Clean up open file descriptors
      SHPClose( hSHP );

#ifdef WITH_DBFAWK
      dbfawk_free_info(fld_info);
      if (sig_info != NULL && sig_info != dbfawk_default_sig  && (sig_info->sig == NULL))
      {
        dbfawk_free_sigs(sig_info);
      }
#endif  // WITH_DBFAWK

      return; // Multipoint type.  Not implemented yet.
      break;

    default:
      DBFClose( hDBF );   // Clean up open file descriptors
      SHPClose( hSHP );

#ifdef WITH_DBFAWK
      dbfawk_free_info(fld_info);
      if (sig_info != NULL && sig_info != dbfawk_default_sig  && (sig_info->sig == NULL))
      {
        dbfawk_free_sigs(sig_info);
      }
#endif  // WITH_DBFAWK

      return; // Unknown type.  Don't know how to process it.
      break;
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

#ifdef WITH_DBFAWK
    dbfawk_free_info(fld_info);
    if (sig_info != NULL && sig_info != dbfawk_default_sig  && (sig_info->sig == NULL))
    {
      dbfawk_free_sigs(sig_info);
    }
#endif  // WITH_DBFAWK

    return;     // The file contains no shapes in our viewport
  }


  // Set a default line width for all maps.  This will most likely
  // be modified for particular maps in later code.
  (void)XSetLineAttributes(XtDisplay(w), gc, 0, LineSolid, CapButt,JoinMiter);


  // NOTE: Setting the color here and in the "else" may not stick if we do more
  //       complex drawing further down like a SteelBlue lake with a black boundary,
  //       or if we have labels turned on which resets our color to black.
  if (weather_alert_flag)     /* XXX */
  {
    char xbm_path[MAX_FILENAME];
    unsigned int _w, _h;
    int _xh, _yh;
    int ret_val;
    FILE *alert_fp = NULL;
    char xbm_filename[MAX_FILENAME];


    // This GC is used for weather alerts (writing to the
    // pixmap: pixmap_alerts) and _was_ used for beam_heading
    // rays, but no longer is.
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

    // Attempt to open the alert filename:  <alert_tag>.xbm (lower-case alert text)
    // to detect whether we have a matching filename for our alert text.
    xastir_snprintf(xbm_filename, sizeof(xbm_filename), "%s", alert->alert_tag);

    // Convert the filename to lower-case
    to_lower(xbm_filename);

    // Construct the complete path/filename
    strcpy(xbm_path, SYMBOLS_DIR);
    xbm_path[sizeof(xbm_path)-1] = '\0';  // Terminate string
    strcat(xbm_path, "/");
    xbm_path[sizeof(xbm_path)-1] = '\0';  // Terminate string
    strcat(xbm_path, xbm_filename);
    xbm_path[sizeof(xbm_path)-1] = '\0';  // Terminate string
    strcat(xbm_path, ".xbm");
    xbm_path[sizeof(xbm_path)-1] = '\0';  // Terminate string

    // Try opening the file
    alert_fp = fopen(xbm_path, "rb");
    if (alert_fp == NULL)
    {
      // Failed to find a matching file:  Instead use the "alert.xbm" file
      xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "alert.xbm");
    }
    else
    {
      // Success:  Close the file pointer
      fclose(alert_fp);
    }

    /* XXX - need to add SVRTSM */


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
  else   /* !weather_alert_flag */
  {
// Are these actually used anymore by the code?  Colors get set later
// when we know more about what we're dealing with.
#ifndef WITH_DBFAWK
    if (lake_flag || river_flag)
    {
      (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x1a]);  // Steel Blue
    }
    else if (path_flag)
    {
      (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]);  // black
    }
    else if (railroad_flag)
    {
      (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x01]);  // purple
    }
    else if (city_flag || school_flag)
    {
      (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x0e]);  // yellow
    }
    else
    {
      (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]);  // black
    }
#endif /* !WITH_DBFAWK */
  }

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

#ifdef WITH_DBFAWK
    dbfawk_free_info(fld_info);
    if (sig_info != NULL && sig_info != dbfawk_default_sig  && (sig_info->sig == NULL))
    {
      dbfawk_free_sigs(sig_info);
    }
#endif  // WITH_DBFAWK

    return;
  }

  // Now instead of looping over all the shapes, search for the ones that
  // are in our viewport and only loop over those
  //fprintf(stderr,"Deciding how to process this file...\n");
  if (weather_alert_flag)     // We're drawing _one_ weather alert shape
  {
    //fprintf(stderr," weather alert flag set...\n");
    if (found_shape != -1)      // Found the record
    {
      //fprintf(stderr,"  found_shape set...\n");
      // just in case we haven't drawn any real maps yet...
      if (!RTree_hitarray)
      {
        //fprintf(stderr,"   mallocing hitarray...\n");
        RTree_hitarray = (int *)malloc(sizeof(int)*1000);
        RTree_hitarray_size=1000;
      }
      CHECKMALLOC(RTree_hitarray);
      RTree_hitarray[0]=found_shape;
      //fprintf(stderr," %s contains alert\n",file);
      nhits=1;
    }
    else    // Didn't find the record
    {
      //fprintf(stderr,"   found_shape is -1...\n");
      nhits=0;
    }
  }
  else    // Draw an entire Shapefile map
  {
    //fprintf(stderr,"   weather_alert_flag not set...\n");
    if (si)
    {
      //fprintf(stderr,"   si is 0x%lx...\n",(unsigned long int) si);
      RTree_hitarray_index=0;
      // the callback will be executed every time the search finds a
      // shape whose bounding box overlaps the viewport.
      nhits = Xastir_RTreeSearch(si->root, &viewportRect,
                                 (void *)RTreeSearchCallback, 0);
      //fprintf(stderr,"Found %d hits in %s\n",nhits,file);
    }
    else
    {
      //fprintf(stderr,"   si not set ...\n");
      // we read the entire shapefile
      nhits=nEntities;
      // fprintf(stderr," %s entirely in view, with %d shapes\n",file,nhits);
    }
  }
  //fprintf(stderr," Done with decision, nhits is %d\n",nhits);

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
#ifdef WITH_DBFAWK
        dbfawk_free_info(fld_info);
        if (sig_info != NULL && sig_info != dbfawk_default_sig  && (sig_info->sig == NULL))
        {
          dbfawk_free_sigs(sig_info);
        }
#endif  // WITH_DBFAWK
        return;
      }
    }


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

    // Have had segfaults before at the SHPReadObject() call
    // when the Shapefile was corrupted.
    //fprintf(stderr,"Before SHPReadObject:%d\n",structure);

    object = SHPReadObject( hSHP, structure );  // Note that each structure can have multiple rings

    //fprintf(stderr,"After SHPReadObject\n");

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

//        if (debug_level & 16)
//            fprintf(stderr,"Calling map_visible_lat_lon on a shape\n");

    if ( map_visible_lat_lon( object->dfYMin,   // Bottom
                              object->dfYMax,   // Top
                              object->dfXMin,   // Left
                              object->dfXMax) )     // Right
    {

      const char *temp;
#ifndef WITH_DBFAWK
      char temp2[100];
#endif /*!WITH_DBFAWK*/
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
#ifdef WITH_DBFAWK
      if (sig_info)
      {
        dbfawk_parse_record(sig_info->prog,hDBF,fld_info,structure);
        if (debug_level & 16)
        {
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
          fprintf(stderr,"label_level=%d ",label_level);
          fprintf(stderr,"label_color=%d\n",label_color);
          // fprintf(stderr,"layer=%d\n",layer);
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


        if (weather_alert_flag)   /* XXX will this fix WX alerts? */
        {
          fill_style = FillStippled;
        }

        skip_it = (map_color_levels && (scale_y > display_level));
        skip_label = (map_color_levels && (scale_y > label_level));

      }
#else   /* WITH_DBFAWK */
      if (draw_filled == 2)
      {
        // "Auto" fill was chosen, but dbfawk support is not
        // compiled in.  Default to vector mode (no polygon
        // fills)
        draw_filled = 0;
      }
#endif /* WITH_DBFAWK */

      /* This case statement is a mess.  Lots of common code could be
         used before the case but currently isn't */

      switch ( nShapeType )
      {


        case SHPT_POINT:
        case SHPT_POINTZ:
          // We hit this case once for each point shape in
          // the file, iff that shape is within our
          // viewport.


          if (debug_level & 16)
          {
            fprintf(stderr,"Found Point Shapefile\n");
          }

          // Read each point, place a label there, and an optional symbol
          //object->padfX
          //object->padfY
          //object->padfZ

//                    if (    mapshots_labels_flag
//                            && map_labels
//                            && (fieldcount >= 3) ) {

          if (!skip_it)      // Need a bracket so we can define
          {
            // some local variables.
            const char *temp = NULL;
            int ok = 1;
            int temp_ok;

#ifdef WITH_DBFAWK
            if (map_labels)
            {
              temp = name;
            }
#else /* !WITH_DBFAWK */
            // If labels are enabled and we have enough
            // fields in the .dbf file, read the label.
            if (map_labels && fieldcount >= 1)
            {
              // Snag the label from the .dbf file
              temp = DBFReadStringAttribute( hDBF, structure, 0 );
            }
#endif /* !WITH_DBFAWK */
            // Convert point to Xastir coordinates
            temp_ok = convert_to_xastir_coordinates(&my_long,
                                                    &my_lat,
                                                    (float)object->padfX[0],
                                                    (float)object->padfY[0]);
            //fprintf(stderr,"%ld %ld\n", my_long, my_lat);

            if (!temp_ok)
            {
              fprintf(stderr,"draw_shapefile_map1: Problem converting from lat/lon\n");
              ok = 0;
              x = 0;
              y = 0;
            }
            else
            {
              // Convert to screen coordinates.  Careful
              // here!  The format conversions you'll need
              // if you try to compress this into two
              // lines will get you into trouble.
              x = my_long - NW_corner_longitude;
              y = my_lat - NW_corner_latitude;
              x = x / scale_x;
              y = y / scale_y;
            }

            if (ok == 1)
            {
              char symbol_table = '/';
              char symbol_id = '.'; /* small x */
              char symbol_over = ' ';

              // Fine-tuned the location here so that
              // the middle of the 'X' would be at the
              // proper pixel.
#ifdef WITH_DBFAWK
              if (*sym)
              {
                symbol(w,0,sym[0],sym[1],sym[2],pixmap,1,x-10,y-10,' ');
              }
              else
              {
                symbol(w, 0, symbol_table, symbol_id, symbol_over, pixmap, 1, x-10, y-10, ' ');
              }
#else // WITH_DBFAWK
              symbol(w, 0, symbol_table, symbol_id, symbol_over, pixmap, 1, x-10, y-10, ' ');
#endif //WITH_DBFAWK

              // Fine-tuned this string so that it is
              // to the right of the 'X' and aligned
              // nicely.
              if (map_labels && !skip_label)
              {
// Labeling of points done here
                draw_nice_string(w, pixmap, 0, x+10, y+5, (char*)temp, 0xf, 0x10, strlen(temp));
                //(void)draw_label_text ( w, x, y, strlen(temp), colors[label_color], (char *)temp);
                //(void)draw_rotated_label_text (w, 90, x+10, y, strlen(temp), colors[label_color], (char *)temp);
              }
            }
          }
          break;



        case SHPT_ARC:
        case SHPT_ARCZ:
          // We hit this case once for each polyline shape
          // in the file, iff at least part of that shape
          // is within our viewport.


          if (debug_level & 16)
          {
            fprintf(stderr,"Found Polylines\n");
          }

// Draw the PolyLines themselves:

          // Default in case we forget to set the line
          // width later:
          (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineSolid, CapButt,JoinMiter);


#ifdef WITH_DBFAWK
          if (!skip_it)
          {
            (void)XSetForeground(XtDisplay(w), gc, colors[color]);
            (void)XSetLineAttributes(XtDisplay (w), gc,
                                     (lanes)?lanes:1,
                                     pattern,
                                     CapButt,JoinMiter);
          }
#else /*!WITH_DBFAWK*/
// Set up width and zoom level for roads
          if (road_flag)
          {
            int lanes = 0;
            int dashed_line = 0;

            if ( mapshots_labels_flag && (fieldcount >= 9) )
            {
              const char *temp;

              temp = DBFReadStringAttribute( hDBF, structure, 8 );    // CFCC Field
              switch (temp[1])
              {
                case '1':   // A1? = Primary road or interstate highway
                  if (map_color_levels && scale_y > 16384)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 16384)
                  {
                    skip_label++;
                  }
                  lanes = 4;
                  (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x04]); // brown
                  break;
                case '2':   // A2? = Primary road w/o limited access, US highways
                  if (map_color_levels && scale_y > 8192)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 8192)
                  {
                    skip_label++;
                  }
                  lanes = 3;
                  (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]); // black
                  break;
                case '3':   // A3? = Secondary road & connecting road, state highways
                  // Skip the road if we're above this zoom level
                  if (map_color_levels && scale_y > 256)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 256)
                  {
                    skip_label++;
                  }
                  lanes = 2;
                  (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]); // black
                  switch (temp[2])
                  {
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                      break;
                    case '7':
                    case '8':
                    default:
                      if (map_color_levels && scale_y > 128)
                      {
                        skip_label++;
                      }
                      break;
                  }
                  break;
                case '4':   // A4? = Local, neighborhood & rural roads, city streets
                  // Skip the road if we're above this zoom level
                  if (map_color_levels && scale_y > 96)
                  {
                    skip_it++;
                  }
                  // Skip labels above this zoom level to keep things uncluttered
                  if (map_color_levels && scale_y > 16)
                  {
                    skip_label++;
                  }
                  lanes = 1;
                  (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x28]); // gray35
                  break;
                case '5':   // A5? = Vehicular trail passable only by 4WD vehicle
                  // Skip the road if we're above this zoom level
                  if (map_color_levels && scale_y > 64)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 16)
                  {
                    skip_label++;
                  }
                  lanes = 1;
                  dashed_line++;
                  (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x28]); // gray35
                  break;
                case '6':   // A6? = Cul-de-sac, traffic circles, access ramp,
                  // service drive, ferry crossing
                  if (map_color_levels && scale_y > 64)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 16)
                  {
                    skip_label++;
                  }

                  switch (temp[2])
                  {
                    case '5':   // Ferry crossing
                      lanes = 2;
                      dashed_line++;
                      (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x08]); // black
                      break;
                    default:
                      lanes = 1;
                      (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x28]); // gray35
                      break;
                  }

                  break;
                case '7':   // A7? = Walkway or pedestrian trail, stairway,
                  // alley, driveway or service road
                  // Skip the road if we're above this zoom level
                  if (map_color_levels && scale_y > 64)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 16)
                  {
                    skip_label++;
                  }
                  lanes = 1;
                  dashed_line++;

                  switch (temp[2])
                  {
                    case '1':   // Walkway or trail for pedestrians
                    case '2':   // Stairway or stepped road for pedestrians
                      (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x0c]); // red
                      break;
                    default:
                      (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x28]); // gray35
                      break;
                  }

                  break;
                default:
                  lanes = 1;
                  if (map_color_levels && scale_y > 64)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 16)
                  {
                    skip_label++;
                  }
                  (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x28]); // gray35
                  break;
              }
            }
            else    // Must not be a mapshots or ESRI Tiger map
            {
              if (fieldcount >= 7)    // Need at least 7 fields if we're snagging #6, else segfault
              {
                lanes = DBFReadIntegerAttribute( hDBF, structure, 6 );

                // In case we guess wrong on the
                // type of file and the lanes are
                // really out of bounds:
                if (lanes < 1 || lanes > 10)
                {
                  //fprintf(stderr,"lanes = %d\n",lanes);
                  lanes = 1;
                }
              }
              (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x28]); // gray35
            }
            if (lanes != 0)
            {
              if (dashed_line)
              {
                (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineOnOffDash, CapButt,JoinMiter);
              }
              else
              {
                (void)XSetLineAttributes (XtDisplay (w), gc, lanes, LineSolid, CapButt,JoinMiter);
              }
            }
            else
            {
              (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineSolid, CapButt,JoinMiter);
            }
          }   // End of road flag portion

// Set up width and zoom levels for water
          else if (river_flag || lake_flag)
          {
            int lanes = 0;
            int dashed_line = 0;
            int glacier_flag = 0;

            if ( mapshots_labels_flag && (fieldcount >= 9) )
            {
              const char *temp;

              temp = DBFReadStringAttribute( hDBF, structure, 8 );    // CFCC Field
              switch (temp[1])
              {
                case '0':   // H0? = Water feature/shoreline
                  // Skip the vector if we're above this zoom level
                  if (map_color_levels && scale_y > 4096)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 16)
                  {
                    skip_label++;
                  }
                  lanes = 0;

                  switch (temp[2])
                  {
                    case '2':   // Intermittent
                      dashed_line++;
                      // Skip the vector if we're above this zoom level
                      if (map_color_levels && scale_y > 64)
                      {
                        skip_it++;
                      }
                      if (map_color_levels && scale_y > 16)
                      {
                        skip_label++;
                      }
                      break;
                    default:
                      break;
                  }

                  break;
                case '1':
                  // Skip the vector if we're above this zoom level
                  if (map_color_levels && scale_y > 256)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 128)
                  {
                    skip_label++;
                  }
                  switch (temp[2])
                  {
                    case '0':
                      if (map_color_levels && scale_y > 16)
                      {
                        skip_label++;
                      }
                      lanes = 1;
                      break;
                    case '1':
                      if (map_color_levels && scale_y > 16)
                      {
                        skip_label++;
                      }
                      lanes = 1;
                      break;
                    case '2':
                      if (map_color_levels && scale_y > 16)
                      {
                        skip_label++;
                      }
                      lanes = 1;
                      dashed_line++;
                      break;
                    case '3':
                      if (map_color_levels && scale_y > 16)
                      {
                        skip_label++;
                      }
                      lanes = 1;
                      break;
                    default:
                      if (map_color_levels && scale_y > 16)
                      {
                        skip_label++;
                      }
                      lanes = 1;
                      break;
                  }
                  break;
                case '2':
                  // Skip the vector if we're above this zoom level
                  if (map_color_levels && scale_y > 256)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 16)
                  {
                    skip_label++;
                  }
                  lanes = 1;

                  switch (temp[2])
                  {
                    case '2':   // Intermittent
                      dashed_line++;
                      break;
                    default:
                      break;
                  }

                  break;
                case '3':
                  // Skip the vector if we're above this zoom level
                  if (map_color_levels && scale_y > 256)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 16)
                  {
                    skip_label++;
                  }
                  lanes = 1;

                  switch (temp[2])
                  {
                    case '2':   // Intermittent
                      dashed_line++;
                      break;
                    default:
                      break;
                  }

                  break;
                case '4':
                  // Skip the vector if we're above this zoom level
                  if (map_color_levels && scale_y > 256)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 16)
                  {
                    skip_label++;
                  }
                  lanes = 1;

                  switch (temp[2])
                  {
                    case '2':   // Intermittent
                      dashed_line++;
                      break;
                    default:
                      break;
                  }

                  break;
                case '5':
                  // Skip the vector if we're above this zoom level
                  if (map_color_levels && scale_y > 256)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 16)
                  {
                    skip_label++;
                  }
                  lanes = 1;
                  break;
                case '6':
                  // Skip the vector if we're above this zoom level
                  if (map_color_levels && scale_y > 256)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 16)
                  {
                    skip_label++;
                  }
                  lanes = 1;
                  break;
                case '7':   // Nonvisible stuff.  Don't draw these
                  skip_it++;
                  skip_label++;
                  break;
                case '8':
                  // Skip the vector if we're above this zoom level
                  if (map_color_levels && scale_y > 256)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 16)
                  {
                    skip_label++;
                  }
                  lanes = 1;

                  switch (temp[2])
                  {
                    case '1':   // Glacier
                      glacier_flag++;
                      break;
                    default:
                      break;
                  }

                  break;
                default:
                  // Skip the vector if we're above this zoom level
                  if (map_color_levels && scale_y > 256)
                  {
                    skip_it++;
                  }
                  if (map_color_levels && scale_y > 16)
                  {
                    skip_label++;
                  }
                  lanes = 1;
                  break;
              }
              if (dashed_line)
              {
                (void)XSetLineAttributes (XtDisplay (w), gc, lanes, LineOnOffDash, CapButt,JoinMiter);
              }
              else
              {
                (void)XSetLineAttributes (XtDisplay (w), gc, lanes, LineSolid, CapButt,JoinMiter);
              }
            }
            else    // We don't know how wide to make it, not a mapshots or ESRI Tiger maps
            {
              if (dashed_line)
              {
                (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineOnOffDash, CapButt,JoinMiter);
              }
              else
              {
                (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineSolid, CapButt,JoinMiter);
              }
            }
            if (glacier_flag)
            {
              (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x0f]);  // white
            }
            else
            {
              (void)XSetForeground(XtDisplay(w), gc, colors[(int)0x1a]);  // Steel Blue
            }
          }   // End of river_flag/lake_flag code

          else    // Set default line width, use whatever color is already defined by this point.
          {
            (void)XSetLineAttributes (XtDisplay (w), gc, 0, LineSolid, CapButt,JoinMiter);
          }
#endif /* !WITH_DBFAWK */

//WE7U
// I'd like to be able to change the color of each GPS track for
// each team in the field.  It'll help to differentiate the tracks
// where they happen to cross.
          /* XXX - WITH_DBFAWK should handle this case too.  Need to add a color
             attribute to the generated dbf file */
          if (gps_flag)
          {
            int jj;
            int done = 0;


            // Fill in the label we'll use later
            xastir_snprintf(gps_label,
                            sizeof(gps_label), "%s",
                            filename);

            // Knock off the "_Color.shp" portion of the
            // label.  Find the last underline character
            // and change it to an end-of-string.
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


            // Check for a color in the filename: i.e.
            // "Team2_Track_Red.shp"
            if (strstr(filenm,"_Red.shp"))
            {
              gps_color = 0x0c; // Red
            }
            else if (strstr(filenm,"_Green.shp"))
            {
//                            gps_color = 0x64; // ForestGreen
              gps_color = 0x23; // Area Green Hi
            }
            else if (strstr(filenm,"_Black.shp"))
            {
              gps_color = 0x08; // black
            }
            else if (strstr(filenm,"_White.shp"))
            {
              gps_color = 0x0f; // white
            }
            else if (strstr(filenm,"_Orange.shp"))
            {
//                            gps_color = 0x06; // orange
//                            gps_color = 0x19; // orange2
//                            gps_color = 0x41; // DarkOrange3 (good medium orange)
              gps_color = 0x62; // orange3 (brighter)
            }
            else if (strstr(filenm,"_Blue.shp"))
            {
              gps_color = 0x03; // cyan
            }
            else if (strstr(filenm,"_Yellow.shp"))
            {
              gps_color = 0x0e; // yellow
            }
            else if (strstr(filenm,"_Purple.shp"))
            {
              gps_color = 0x0b; // mediumorchid
            }
            else    // Default color
            {
              gps_color = 0x0c; // Red
            }

            // Set the color for the arc's
            (void)XSetForeground(XtDisplay(w), gc, colors[gps_color]);

            // Make the track nice and wide: Easy to
            // see.
//                        (void)XSetLineAttributes (XtDisplay (w), gc, 3, LineSolid, CapButt,JoinMiter);
            (void)XSetLineAttributes (XtDisplay (w), gc, 3, LineOnOffDash, CapButt,JoinMiter);
//                        (void)XSetLineAttributes (XtDisplay (w), gc, 3, LineDoubleDash, CapButt,JoinMiter);

          }   // End of gps flag portion


          index = 0;  // Index into our own points array.
          // Tells how many points we've
          // collected so far.


          if (ok_to_draw && !skip_it)
          {

            // Read the vertices for each vector now

            for (ring = 0; ring < object->nVertices; ring++ )
            {
              int temp_ok;

              ok = 1;

              //fprintf(stderr,"\t%d:%g %g\t", ring, object->padfX[ring], object->padfY[ring] );
              // Convert to Xastir coordinates
              temp_ok = convert_to_xastir_coordinates(&my_long,
                                                      &my_lat,
                                                      (float)object->padfX[ring],
                                                      (float)object->padfY[ring]);
              //fprintf(stderr,"%ld %ld\n", my_long, my_lat);

              if (!temp_ok)
              {
                fprintf(stderr,"draw_shapefile_map2: Problem converting from lat/lon\n");
                ok = 0;
                x = 0;
                y = 0;
              }
              else
              {
                // Convert to screen coordinates.  Careful
                // here!  The format conversions you'll need
                // if you try to compress this into two
                // lines will get you into trouble.
                x = my_long - NW_corner_longitude;
                y = my_lat - NW_corner_latitude;
                x = x / scale_x;
                y = y / scale_y;


                // Save the endpoints of the first line
                // segment for later use in label rotation
                if (ring == 0)
                {
                  // Save the first set of screen coordinates
                  x0 = (int)x;
                  y0 = (int)y;
                }
                else if (ring == 1)
                {
                  // Save the second set of screen coordinates
                  x1 = (int)x;
                  y1 = (int)y;
                }
              }

              if (ok == 1)
              {
                // XDrawLines uses 16-bit unsigned
                // integers (shorts).  Make sure we
                // stay within the limits.
                points[index].x = l16(x);
                points[index].y = l16(y);
                //fprintf(stderr,"%d %d\t", points[index].x, points[index].y);
                index++;
              }
              if (index > high_water_mark_index)
              {
                high_water_mark_index = index;
              }

              if (index >= MAX_MAP_POINTS)
              {
                index = MAX_MAP_POINTS - 1;
                fprintf(stderr,"Trying to overrun the points array: SHPT_ARC, index=%d\n",index);
              }
            }   // End of "for" loop for polyline points
          }


          if (ok_to_draw && !skip_it)
          {
            (void)XDrawLines(XtDisplay(w),
                             pixmap,
                             gc,
                             points,
                             l16(index),
                             CoordModeOrigin);
          }


// Figure out and draw the labels for PolyLines.  Note that we later
// determine whether we want to draw the label at all.  Move all
// code possible below that decision point to keep everything fast.
// Don't do unnecessary calculations if we're not going to draw the
// label.

#ifdef WITH_DBFAWK
          temp = (gps_flag)?gps_label:name;
#else /* !WITH_DBFAWK */
          // We're done with drawing the arc's.  Draw the
          // labels in this next section.
          //
          temp = "";
          if (       !skip_label
                     && !skip_it
                     && map_labels
                     && road_flag)
          {
            char a[2],b[2],c[2];

            if ( (mapshots_labels_flag) && (fieldcount >= 8) )
            {
              char temp3[3];
              char temp4[31];
              char temp5[5];
              char temp6[3];

              temp = DBFReadStringAttribute( hDBF, structure, 4 );
              xastir_snprintf(temp3,sizeof(temp3),"%s",temp);
              temp = DBFReadStringAttribute( hDBF, structure, 5 );
              xastir_snprintf(temp4,sizeof(temp4),"%s",temp);
              temp = DBFReadStringAttribute( hDBF, structure, 6 );
              xastir_snprintf(temp5,sizeof(temp5),"%s",temp);
              temp = DBFReadStringAttribute( hDBF, structure, 7 );
              xastir_snprintf(temp6,sizeof(temp6),"%s",temp);

              // Take care to not insert spaces if
              // some of the strings are empty.
              if (strlen(temp3) != 0)
              {
                xastir_snprintf(a,sizeof(a)," ");
              }
              else
              {
                a[0] = '\0';
              }
              if (strlen(temp4) != 0)
              {
                xastir_snprintf(b,sizeof(b)," ");
              }
              else
              {
                b[0] = '\0';
              }
              if (strlen(temp5) != 0)
              {
                xastir_snprintf(c,sizeof(c)," ");
              }
              else
              {
                c[0] = '\0';
              }

              xastir_snprintf(temp2,sizeof(temp2),"%s%s%s%s%s%s%s",
                              temp3,a,temp4,b,temp5,c,temp6);
              temp = temp2;
            }
            else if (fieldcount >=10)    // Need at least 10 fields if we're snagging #9, else segfault
            {
              // For roads, we need to use SIGN1 if it exists, else use DESCRIP if it exists.
              temp = DBFReadStringAttribute( hDBF, structure, 9 );    // SIGN1
            }
            if ( (temp == NULL) || (strlen(temp) == 0) )
            {
              if (fieldcount >=13)    // Need at least 13 fields if we're snagging #12, else segfault
              {
                temp = DBFReadStringAttribute( hDBF, structure, 12 );  // DESCRIP
              }
              else
              {
                temp = NULL;
              }
            }
          }
          else if (!skip_label
                   && map_labels
                   && !skip_it
                   && (lake_flag || river_flag) )
          {

            if ( mapshots_labels_flag && river_flag && (fieldcount >= 8) )
            {
              char temp3[3];
              char temp4[31];
              char temp5[5];
              char temp6[3];


              temp = DBFReadStringAttribute( hDBF, structure, 4 );
              xastir_snprintf(temp3,sizeof(temp3),"%s",temp);
              temp = DBFReadStringAttribute( hDBF, structure, 5 );
              xastir_snprintf(temp4,sizeof(temp4),"%s",temp);
              temp = DBFReadStringAttribute( hDBF, structure, 6 );
              xastir_snprintf(temp5,sizeof(temp5),"%s",temp);
              temp = DBFReadStringAttribute( hDBF, structure, 7 );
              xastir_snprintf(temp6,sizeof(temp6),"%s",temp);
              xastir_snprintf(temp2,sizeof(temp2),"%s %s %s %s",
                              temp3,temp4,temp5,temp6);
              temp = temp2;
            }
            else if (mapshots_labels_flag && lake_flag && (fieldcount >= 4) )
            {
              temp = DBFReadStringAttribute( hDBF, structure, 3 );
            }
            else if (fieldcount >=14)    // Need at least 14 fields if we're snagging #13, else segfault
            {
              temp = DBFReadStringAttribute( hDBF, structure, 13 );   // PNAME (rivers)
            }
            else
            {
              temp = NULL;
            }
          }


          // First we need to convert over to using the
          // temp2 variable, which is changeable.  Make
          // temp point to it.  temp may already be
          // pointing to the temp2 variable.

          if (temp != temp2)
          {
            // temp points to an unchangeable string

            if (temp != NULL)   // NOAA interstates file has a NULL at this point
            {
              // Copy the string so we can change it
              xastir_snprintf(temp2,sizeof(temp2),"%s",temp);
              temp = temp2;                       // Point temp to it (for later use)
            }
            else
            {
              temp2[0] = '\0';
            }
          }
          else    // We're already set to work on temp2!
          {
          }


          if ( map_labels && gps_flag )
          {
            // We're drawing GPS info.  Use gps_label,
            // overriding anything set before.
            xastir_snprintf(temp2,
                            sizeof(temp2),
                            "%s",
                            gps_label);
          }


          // Change "United States Highway 2" into "US 2"
          // Look for substring at start of string
          if ( strstr(temp2,"United States Highway") == temp2 )
          {
            int index;
            // Convert to "US"
            temp2[1] = 'S';  // Add an 'S'
            index = 2;
            while (temp2[index+19] != '\0')
            {
              temp2[index] = temp2[index+19];
              index++;
            }
            temp2[index] = '\0';
          }
          else    // Change "State Highway 204" into "State 204"
          {
            // Look for substring at start of string
            if ( strstr(temp2,"State Highway") == temp2 )
            {
              int index;
              // Convert to "State"
              index = 5;
              while (temp2[index+8] != '\0')
              {
                temp2[index] = temp2[index+8];
                index++;
              }
              temp2[index] = '\0';
            }
            else    // Change "State Route 2" into "State 2"
            {
              // Look for substring at start of string
              if ( strstr(temp2,"State Route") == temp2 )
              {
                int index;
                // Convert to "State"
                index = 5;
                while (temp2[index+6] != '\0')
                {
                  temp2[index] = temp2[index+6];
                  index++;
                }
                temp2[index] = '\0';
              }
            }
          }
#endif /* !WITH_DBFAWK */
          if ( (temp != NULL)
               && (strlen(temp) != 0)
               && map_labels
               && !skip_it
               && !skip_label )
          {
            int temp_ok;

            ok = 1;

            // Convert to Xastir coordinates
            temp_ok = convert_to_xastir_coordinates(&my_long,
                                                    &my_lat,
                                                    (float)object->padfX[0],
                                                    (float)object->padfY[0]);
            //fprintf(stderr,"%ld %ld\n", my_long, my_lat);

            if (!temp_ok)
            {
              fprintf(stderr,"draw_shapefile_map3: Problem converting from lat/lon\n");
              ok = 0;
              x = 0;
              y = 0;
            }
            else
            {
              // Convert to screen coordinates.  Careful
              // here!  The format conversions you'll need
              // if you try to compress this into two
              // lines will get you into trouble.
              x = my_long - NW_corner_longitude;
              y = my_lat - NW_corner_latitude;
              x = l16(x / scale_x);
              y = l16(y / scale_y);
            }

            if (ok == 1 && ok_to_draw)
            {

              int new_label = 1;
              int mod_number;

              // Set up the mod_number, which is used
              // below to determine how many of each
              // identical label are skipped at each
              // zoom level.

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

              if      (scale_y == 1)
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

              // Check whether we've written out this string
              // already:  Look for a match in our linked list

// The problem with this method is that we might get strings
// "written" at the extreme top or right edge of the display, which
// means the strings wouldn't be visible, but Xastir thinks that it
// wrote the string out visibly.  To partially counteract this I've
// set it up to write only some of the identical strings.  This
// still doesn't help in the cases where a street only comes in from
// the top or right and doesn't have an intersection with another
// street (and therefore another label) within the view.

              // Hash index is just the first
              // character.  Tried using lower 6 bits
              // of first two chars and lower 7 bits
              // of first two chars but the result was
              // slower than just using the first
              // character.
              hash_index = (uint8_t)(temp[0]);

              ptr2 = label_hash[hash_index];
              while (ptr2 != NULL)     // Step through the list
              {
                // Check 2nd character (fast!)
                if ( (uint8_t)(ptr2->label[1]) == (uint8_t)(temp[1]) )
                {
                  if (strcasecmp(ptr2->label,temp) == 0)      // Found a match
                  {
                    //fprintf(stderr,"Found a match!\t%s\n",temp);
                    new_label = 0;
                    ptr2->found = ptr2->found + 1;  // Increment the "found" quantity

// We change this "mod" number based on zoom level, so that long
// strings don't overwrite each other, and so that we don't get too
// many or too few labels drawn.  This will cause us to skip
// intersections (the tiger files appear to have a label at each
// intersection).  Between rural and urban areas, this method might
// not work well.  Urban areas have few intersections, so we'll get
// fewer labels drawn.
// A better method might be to check the screen location for each
// one and only write the strings if they are far enough apart, and
// only count a string as written if the start of it is onscreen and
// the angle is correct for it to be written on the screen.

                    // Draw a number of labels
                    // appropriate for the zoom
                    // level.
// Labeling: Skip label logic
                    if ( ((ptr2->found - 1) % mod_number) != 0)
                    {
                      skip_label++;
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

              if (!skip_label)    // Draw the string
              {

                // Compute the label rotation angle
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

                //fprintf(stderr,"Y: %f\tX: %f\tAngle: %f ==> ",diff_Y,diff_X,angle);

                if ( angle > 90.0 )
                {
                  angle += 180.0;
                }
                if ( angle >= 360.0 )
                {
                  angle -= 360.0;
                }

                //fprintf(stderr,"%f\t%s\n",angle,temp);

// Labeling of polylines done here

//                              (void)draw_label_text ( w, x, y, strlen(temp), colors[label_color], (char *)temp);
                if (gps_flag)
                {
                  (void)draw_rotated_label_text (w,
                                                 //(int)angle,
                                                 -90,    // Horizontal, easiest to read
                                                 x,
                                                 y,
                                                 strlen(temp),
                                                 colors[gps_color],
                                                 (char *)temp,
                                                 font_size);
                }
                else
                {
                  (void)draw_rotated_label_text(w,
                                                (int)angle,
                                                x,
                                                y,
                                                strlen(temp),
                                                colors[label_color],
                                                (char *)temp,
                                                font_size);
                }
              }

              if (new_label)
              {

                // Create a new record for this string
                // and add it to the head of the list.
                // Make sure to "free" this linked
                // list.
                //fprintf(stderr,"Creating a new record: %s\n",temp);
                ptr2 = (label_string *)malloc(sizeof(label_string));
                CHECKMALLOC(ptr2);

                memcpy(ptr2->label, temp, sizeof(ptr2->label));
                ptr2->label[sizeof(ptr2->label)-1] = '\0';  // Terminate string
                ptr2->found = 1;

                // We use first character of string
                // as our hash index.
                hash_index = temp[0];

                ptr2->next = label_hash[hash_index];
                label_hash[hash_index] = ptr2;
                //if (label_hash[hash_index]->next == NULL)
                //    fprintf(stderr,"only one record\n");
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

#ifdef WITH_DBFAWK
          // User requested filled polygons with stippling.
          // Set the stipple now.  need to do here, because if
          // done earlier the labels get stippled, too.
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
#endif  // WITH_DBFAWK

          // Each polygon can be made up of multiple
          // rings, and each ring has multiple points that
          // define it.  We hit this case once for each
          // polygon shape in the file, iff at least part
          // of that shape is within our viewport.

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
//
//
// Shapefiles also allow identical points to be next to each other
// in the vertice list.  We should look for that and get rid of
// duplicate vertices.


//if (object->nParts > 1)
//fprintf(stderr,"Number of parts: %d\n", object->nParts);


// Unfortunately, for Polygon shapes we must make one pass through
// the entire set of rings to see if we have any "hole" rings (as
// opposed to "fill" rings).  If we have any "hole" rings, we must
// handle the drawing of the Shape quite differently.
//
// Read the vertices for each ring in the Shape.  Test whether we
// have a hole ring.  If so, save the ring index away for the next
// step when we actually draw the shape.

          polygon_hole_flag = 0;

          // Allocate storage for a flag for each ring in
          // this Shape.
          // !!Remember to free this storage later!!
          polygon_hole_storage = (int *)malloc(object->nParts*sizeof(int));
          CHECKMALLOC(polygon_hole_storage);

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
              case  1:    // It's a fill ring
                // Do nothing for these two cases
                // except clear the flag in our
                // storage
                polygon_hole_storage[ring] = 0;
                break;
              case -1:    // It's a hole ring
                // Add it to our list of hole rings
                // here and set a flag.  That way we
                // won't have to run through
                // SHPRingDir_2d again in the next
                // loop.
                polygon_hole_flag++;
                polygon_hole_storage[ring] = 1;
//                                fprintf(stderr, "Ring %d/%d is a polygon hole\n",
//                                    ring,
//                                    object->nParts);
                break;
            }
          }
// We're done with the initial run through the vertices of all
// rings.  We now know which rings are fills and which are holes and
// have recorded the data.

// Speedup:  Only loop through the vertices once, by determining
// hole/fill polygons as we go.


//WE7U3
          // Speedup:  If not draw_filled, then don't go
          // through the math and region code.  Set the
          // flag to zero so that we won't do all the math
          // and the regions.
          if (!map_color_fill || !draw_filled)
          {
            polygon_hole_flag = 0;
          }

          if (polygon_hole_flag)
          {
            XRectangle rectangle;
            long width, height;
            double top_ll, left_ll, bottom_ll, right_ll;
            int temp_ok;


//                        fprintf(stderr, "%s:Found %d hole rings in shape %d\n",
//                            file,
//                            polygon_hole_flag,
//                            structure);

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


            // Create three regions and rotate between
            // them, due to the XSubtractRegion()
            // needing three parameters.  If we later
            // find that two of the parameters can be
            // repeated, we can simplify our code.
            // We'll rotate through them mod 3.

            temp_region1 = 0;

            // Create empty region
            region[temp_region1] = XCreateRegion();

            // Draw a rectangular clip-mask inside the
            // Region.  Use the same extents as the full
            // Shape.

            // Set up the real sizes from the Shape
            // extents.
            top_ll    = object->dfYMax;
            left_ll   = object->dfXMin;
            bottom_ll = object->dfYMin;
            right_ll  = object->dfXMax;

            // Convert point to Xastir coordinates:
            temp_ok = convert_to_xastir_coordinates(&my_long,
                                                    &my_lat,
                                                    left_ll,
                                                    top_ll);
            //fprintf(stderr,"%ld %ld\n", my_long, my_lat);

            if (!temp_ok)
            {
              fprintf(stderr,"draw_shapefile_map4: Problem converting from lat/lon\n");
              ok = 0;
              x = 0;
              y = 0;
            }
            else
            {
              // Convert to screen coordinates.  Careful
              // here!  The format conversions you'll need
              // if you try to compress this into two
              // lines will get you into trouble.
              x = my_long - NW_corner_longitude;
              y = my_lat - NW_corner_latitude;
              x = x / scale_x;
              y = y / scale_y;


              // Here we check for really wacko points that will cause problems
              // with the X drawing routines, and fix them.
              if (x > 1700l)
              {
                x = 1700l;
              }
              if (x <    0l)
              {
                x =    0l;
              }
              if (y > 1700l)
              {
                y = 1700l;
              }
              if (y <    0l)
              {
                y =    0l;
              }
            }

            // Convert points to Xastir coordinates
            temp_ok = convert_to_xastir_coordinates(&my_long,
                                                    &my_lat,
                                                    right_ll,
                                                    bottom_ll);
            //fprintf(stderr,"%ld %ld\n", my_long, my_lat);

            if (!temp_ok)
            {
              fprintf(stderr,"draw_shapefile_map5: Problem converting from lat/lon\n");
              ok = 0;
              width = 0;
              height = 0;
            }
            else
            {
              // Convert to screen coordinates.  Careful
              // here!  The format conversions you'll need
              // if you try to compress this into two
              // lines will get you into trouble.
              width = my_long - NW_corner_longitude;
              height = my_lat - NW_corner_latitude;
              width = width / scale_x;
              height = height / scale_y;

              // Here we check for really wacko points that will cause problems
              // with the X drawing routines, and fix them.
              if (width  > 1700l)
              {
                width  = 1700l;
              }
              if (width  <    1l)
              {
                width  =    1l;
              }
              if (height > 1700l)
              {
                height = 1700l;
              }
              if (height <    1l)
              {
                height =    1l;
              }
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

//fprintf(stderr,"*** Rectangle: %d,%d %dx%d\n", rectangle.x, rectangle.y, rectangle.width, rectangle.height);

            // Create the initial region containing a
            // filled rectangle.
            XUnionRectWithRegion(&rectangle,
                                 region[temp_region1],
                                 region[temp_region1]);

            // Create a region for each set of hole
            // vertices (CCW rotation of the vertices)
            // and subtract each from the rectangle
            // region.
            for (ring = 0; ring < object->nParts; ring++ )
            {
              int endpoint;
              int on_screen;


              if (polygon_hole_storage[ring] == 1)
              {
                // It's a hole polygon.  Cut the
                // hole out of our rectangle region.
                int num_vertices = 0;
//                              int nVertStart;


//                              nVertStart = object->panPartStart[ring];

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
                //fprintf(stderr,"Endpoint %d\n", endpoint);
                //fprintf(stderr,"Vertices: %d\n", endpoint - object->panPartStart[ring]);

                i = 0;  // i = Number of points to draw for one ring
                on_screen = 0;

                // index = ptr into the shapefile's array of points
                for (index = object->panPartStart[ring]; index < endpoint; )
                {
                  int temp_ok;


                  // Get vertice and convert to Xastir coordinates
                  temp_ok = convert_to_xastir_coordinates(&my_long,
                                                          &my_lat,
                                                          (float)object->padfX[index],
                                                          (float)object->padfY[index]);

                  //fprintf(stderr,"%lu %lu\t", my_long, my_lat);

                  if (!temp_ok)
                  {
                    fprintf(stderr,"draw_shapefile_map6: Problem converting from lat/lon\n");
                    ok = 0;
                    x = 0;
                    y = 0;
                  }
                  else
                  {
                    // Convert to screen coordinates.  Careful
                    // here!  The format conversions you'll need
                    // if you try to compress this into two
                    // lines will get you into trouble.
                    x = my_long - NW_corner_longitude;
                    y = my_lat - NW_corner_latitude;
                    x = x / scale_x;
                    y = y / scale_y;


                    //fprintf(stderr,"%ld %ld\t\t", x, y);

                    // Here we check for really
                    // wacko points that will
                    // cause problems with the X
                    // drawing routines, and fix
                    // them.  Increment
                    // on_screen if any of the
                    // points might be on
                    // screen.
                    if (x >  1700l)
                    {
                      x =  1700l;
                    }
                    else if (x < 0l)
                    {
                      x = 0l;
                    }
                    else
                    {
                      on_screen++;
                    }

                    if (y >  1700l)
                    {
                      y =  1700l;
                    }
                    else if (y < 0l)
                    {
                      y = 0l;
                    }
                    else
                    {
                      on_screen++;
                    }

                    points[i].x = l16(x);
                    points[i].y = l16(y);

                    if (on_screen)
                    {
//    fprintf(stderr,"%d x:%d y:%d\n",
//        i,
//        points[i].x,
//        points[i].y);
                    }
                    i++;
                  }
                  index++;

                  if (index > high_water_mark_index)
                  {
                    high_water_mark_index = index;
                  }

                  if (index > endpoint)
                  {
                    index = endpoint;
                    fprintf(stderr,"Trying to run past the end of shapefile array: index=%d\n",index);
                  }
                }   // End of converting vertices for a ring


                // Create and subtract the region
                // only if it might be on screen.
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

                  // Subtract region2 from region1 and
                  // put the result into region3.
//fprintf(stderr, "Subtracting region\n");
                  XSubtractRegion(region[temp_region1],
                                  region[temp_region2],
                                  region[temp_region3]);

                  // Get rid of the two regions we no
                  // longer need
                  XDestroyRegion(region[temp_region1]);
                  XDestroyRegion(region[temp_region2]);

                  // Indicate the final result region for
                  // the next iteration or the exit of the
                  // loop.
                  temp_region1 = temp_region3;
                }
              }
            }

            // region[temp_region1] now contains a
            // clip-mask of the original polygon with
            // holes cut out of it (swiss-cheese
            // rectangle).

            // Create temporary GC.  It looks like we
            // don't need this to create the regions,
            // but we'll need it when we draw the filled
            // polygons onto the map pixmap using the
            // final region as a clip-mask.

// Offsets?
// XOffsetRegion

//                        gc_temp_values.function = GXcopy;
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
          }
//WE7U3
////////////////////////////////////////////////////////////////////////


          // Read the vertices for each ring in this Shape
          for (ring = 0; ring < object->nParts; ring++ )
          {
            int endpoint;
#ifndef WITH_DBFAWK
            int glacier_flag = 0;
            const char *temp;

            if (lake_flag || river_flag)
            {
              if ( mapshots_labels_flag && (fieldcount >= 3) )
              {
                temp = DBFReadStringAttribute( hDBF, structure, 2 );    // CFCC Field
                switch (temp[1])
                {
                  case '8':   // Special water feature
                    switch(temp[2])
                    {
                      case '1':
                        glacier_flag++;  // Found a glacier
                        break;
                      default:
                        break;
                    }
                    break;
                  default:
                    break;
                }
              }
            }
#endif /* !WITH_DBFAWK */
            //fprintf(stderr,"Ring: %d\t\t", ring);

            if ( (ring+1) < object->nParts)
            {
              endpoint = object->panPartStart[ring+1];
            }
            //else endpoint = object->nVertices;
            else
            {
              endpoint = object->panPartStart[0] + object->nVertices;
            }

            //fprintf(stderr,"Endpoint %d\n", endpoint);
            //fprintf(stderr,"Vertices: %d\n", endpoint - object->panPartStart[ring]);

            i = 0;  // i = Number of points to draw for one ring
            // index = ptr into the shapefile's array of points
            for (index = object->panPartStart[ring]; index < endpoint; )
            {
              int temp_ok;

              ok = 1;

              //fprintf(stderr,"\t%d:%g %g\t", index, object->padfX[index], object->padfY[index] );

              // Get vertice and convert to Xastir coordinates
              temp_ok = convert_to_xastir_coordinates(&my_long,
                                                      &my_lat,
                                                      (float)object->padfX[index],
                                                      (float)object->padfY[index]);

              //fprintf(stderr,"%lu %lu\t", my_long, my_lat);

              if (!temp_ok)
              {
                fprintf(stderr,"draw_shapefile_map7: Problem converting from lat/lon\n");
                ok = 0;
                x = 0;
                y = 0;
                index++;
              }
              else
              {
                // Convert to screen coordinates.  Careful
                // here!  The format conversions you'll need
                // if you try to compress this into two
                // lines will get you into trouble.
                x = my_long - NW_corner_longitude;
                y = my_lat - NW_corner_latitude;
                x = x / scale_x;
                y = y / scale_y;


                //fprintf(stderr,"%ld %ld\t\t", x, y);

                // Here we check for really wacko points that will cause problems
                // with the X drawing routines, and fix them.
                points[i].x = l16(x);
                points[i].y = l16(y);
                i++;    // Number of points to draw

                if (i > high_water_mark_i)
                {
                  high_water_mark_i = i;
                }


                if (i >= MAX_MAP_POINTS)
                {
                  i = MAX_MAP_POINTS - 1;
                  fprintf(stderr,"Trying to run past the end of our internal points array: i=%d\n",i);
                }

                //fprintf(stderr,"%d %d\t", points[i].x, points[i].y);

                index++;

                if (index > high_water_mark_index)
                {
                  high_water_mark_index = index;
                }

                if (index > endpoint)
                {
                  index = endpoint;
                  fprintf(stderr,"Trying to run past the end of shapefile array: index=%d\n",index);
                }
              }
            }

            if ( (i >= 3)
                 && (ok_to_draw && !skip_it)
                 && ( !draw_filled || !map_color_fill || (draw_filled && polygon_hole_storage[ring] == 0) ) )
            {
              // We have a polygon to draw!
//WE7U3
              if ((!draw_filled || !map_color_fill) && polygon_hole_storage[ring] == 1)
              {
                // We have a hole drawn as unfilled.
                // Draw as a black dashed line.
#ifdef WITH_DBFAWK
                (void)XSetForeground(XtDisplay(w), gc, colors[color]);
#else   // WITH_DBFAWK
                (void)XSetForeground(XtDisplay(w), gc, colors[0x08]); // black for border
#endif  // WITH_DBFAWK
                (void)XSetLineAttributes (XtDisplay (w),
                                          gc,
                                          0,
                                          LineOnOffDash,
                                          CapButt,
                                          JoinMiter);
                (void)XDrawLines(XtDisplay(w),
                                 pixmap,
                                 gc,
                                 points,
                                 l16(i),
                                 CoordModeOrigin);
                (void)XSetLineAttributes (XtDisplay (w),
                                          gc,
                                          0,
                                          LineSolid,
                                          CapButt,
                                          JoinMiter);
              }
              else if (quad_overlay_flag)
              {
                (void)XDrawLines(XtDisplay(w),
                                 pixmap,
                                 gc,
                                 points,
                                 l16(i),
                                 CoordModeOrigin);
              }
              /* old glacier, lake and river code was identical
                 with the exception of what color to use! */
#ifdef WITH_DBFAWK
              else if (!weather_alert_flag)
              {
                /* color is already set by dbfawk(?) */
                /* And so are lanes and pattern.  Let's
                   use what was specified. */
                (void)XSetLineAttributes(XtDisplay(w),
                                         gc,
                                         (lanes)?lanes:1,
                                         pattern,
                                         CapButt,
                                         JoinMiter);
#else /* !WITH_DBFAWK */
              else if (glacier_flag||lake_flag||river_flag)
              {
                int color = (glacier_flag)?0x0f:
                            (lake_flag||river_flag)?0x1a:8;
#endif /* !WITH_DBFAWK */
                (void)XSetForeground(XtDisplay(w), gc, colors[color]);
                if (map_color_fill && draw_filled)
                {
                  if (polygon_hole_flag)
                  {
#ifdef WITH_DBFAWK
                    (void)XSetForeground(XtDisplay(w), gc_temp, colors[fill_color]);
#else   // WITH_DBFAWK
                    (void)XSetForeground(XtDisplay(w), gc_temp, colors[color]);
#endif  // WITH_DBFAWK
                    if (i >= 3)
                    {
                      (void)XFillPolygon(XtDisplay(w),
                                         pixmap,
                                         gc_temp,
                                         points,
                                         i,
                                         Nonconvex,
                                         CoordModeOrigin);
                    }
                    else
                    {
                      fprintf(stderr,
                              "draw_shapefile_map:Too few points:%d, Skipping XFillPolygon()",
                              npoints);
                    }
                  }
                  else   /* no holes in this polygon */
                  {
                    if (i >= 3)
                    {
                      /* draw the filled polygon */
#ifdef WITH_DBFAWK
                      (void)XSetForeground(XtDisplay(w), gc, colors[fill_color]);
#endif  // WITH_DBFAWK
                      (void)XFillPolygon(XtDisplay(w),
                                         pixmap,
                                         gc,
                                         points,
                                         i,
                                         Nonconvex,
                                         CoordModeOrigin);
                    }
                    else
                    {
                      fprintf(stderr,
                              "draw_shapefile_map:Too few points:%d, Skipping XFillPolygon()",
                              npoints);
                    }
                  }
                }
                /* draw the polygon border */
                (void)XSetForeground(XtDisplay(w), gc, colors[color]);
                (void)XSetFillStyle(XtDisplay(w), gc, FillSolid);
                (void)XDrawLines(XtDisplay(w),
                                 pixmap,
                                 gc,
                                 points,
                                 l16(i),
                                 CoordModeOrigin);
              }
              else if (weather_alert_flag)
              {
                (void)XSetFillStyle(XtDisplay(w), gc_tint, FillStippled);
// We skip the hole/fill thing for these?

                if (i >= 3)
                {
                  (void)XFillPolygon(XtDisplay(w),
                                     pixmap_alerts,
                                     gc_tint,
                                     points,
                                     i,
                                     Nonconvex,
                                     CoordModeOrigin);
                }
                else
                {
                  fprintf(stderr,
                          "draw_shapefile_map:Too few points:%d, Skipping XFillPolygon()",
                          npoints);
                }

                (void)XSetFillStyle(XtDisplay(w), gc_tint, FillSolid);
                (void)XDrawLines(XtDisplay(w),
                                 pixmap_alerts,
                                 gc_tint,
                                 points,
                                 l16(i),
                                 CoordModeOrigin);
              }
              else if (map_color_fill && draw_filled)    // Land masses?
              {
                if (polygon_hole_flag)
                {
#ifndef WITH_DBFAWK
                  /* city_flag not needed WITH_DBFAWK */
                  if (city_flag)
                  {
                    (void)XSetForeground(XtDisplay(w), gc_temp, GetPixelByName(w,"RosyBrown"));  // RosyBrown, duh
                  }
                  else
                  {
                    (void)XSetForeground(XtDisplay(w), gc_temp, colors[0xff]);  // grey
                  }
#else   // WITH_DBFAWK
                  (void)XSetForeground(XtDisplay(w), gc_temp, colors[fill_color]);
#endif /* WITH_DBFAWK */
                  if (i >= 3)
                  {
                    (void)XFillPolygon(XtDisplay(w),
                                       pixmap,
                                       gc_temp,
                                       points,
                                       i,
                                       Nonconvex,
                                       CoordModeOrigin);
                  }
                  else
                  {
                    fprintf(stderr,
                            "draw_shapefile_map:Too few points:%d, Skipping XFillPolygon()",
                            npoints);
                  }
                }
                else   /* no polygon hole */
                {
#ifndef WITH_DBFAWK
                  if (city_flag)
                  {
                    (void)XSetForeground(XtDisplay(w), gc, GetPixelByName(w,"RosyBrown"));  // RosyBrown, duh
                  }
                  else
                  {
                    (void)XSetForeground(XtDisplay(w), gc, colors[0xff]);  // grey
                  }
#else   // WITH_DBFAWK
                  (void)XSetForeground(XtDisplay(w), gc, colors[fill_color]);
#endif /* !WITH_DFBAWK */
                  if (i >= 3)
                  {
                    (void)XFillPolygon(XtDisplay (w),
                                       pixmap,
                                       gc,
                                       points,
                                       i,
                                       Nonconvex,
                                       CoordModeOrigin);
                  }
                  else
                  {
                    fprintf(stderr,
                            "draw_shapefile_map:Too few points:%d, Skipping XFillPolygon()",
                            npoints);
                  }
                }

#ifdef WITH_DBFAWK
                (void)XSetForeground(XtDisplay(w), gc, colors[color]); // border color
#endif  // WITH_DBFAWK

                // Draw a thicker border for city boundaries
#ifndef WITH_DBFAWK
                if (city_flag)
                {
                  if (scale_y <= 64)
                  {
                    (void)XSetLineAttributes(XtDisplay(w), gc, 2, LineSolid, CapButt,JoinMiter);
                  }
                  else if (scale_y <= 128)
                  {
                    (void)XSetLineAttributes(XtDisplay(w), gc, 1, LineSolid, CapButt,JoinMiter);
                  }
                  else
                  {
                    (void)XSetLineAttributes(XtDisplay(w), gc, 0, LineSolid, CapButt,JoinMiter);
                  }

                  (void)XSetForeground(XtDisplay(w), gc, colors[0x14]); // lightgray for border
                }
                else
                {
                  (void)XSetForeground(XtDisplay(w), gc, colors[0x08]); // black for border
                }
#else   // WITH_DBFAWK
                (void)XSetForeground(XtDisplay(w), gc, colors[color]); // border
                (void)XSetFillStyle(XtDisplay(w), gc, FillSolid);
#endif /* WITH_DBFAWK */

                (void)XDrawLines(XtDisplay(w),
                                 pixmap,
                                 gc,
                                 points,
                                 l16(i),
                                 CoordModeOrigin);
              }
              else    // Use whatever color is defined by this point.
              {
                (void)XSetLineAttributes(XtDisplay(w), gc, 0, LineSolid, CapButt,JoinMiter);
                (void)XSetFillStyle(XtDisplay(w), gc, FillSolid);
                (void)XDrawLines(XtDisplay(w),
                                 pixmap,
                                 gc,
                                 points,
                                 l16(i),
                                 CoordModeOrigin);
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


////////////////////////////////////////////////////////////////////////////////////////////////////
// Done with drawing shapes, now draw labels
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WITH_DBFAWK
          temp = name;
          // Set fill style back to defaults, or labels will get
          // stippled along with polygons!
          XSetFillStyle(XtDisplay(w), gc, FillSolid);

#else /* !WITH_DBFAWK */
          /* labels come from dbfawk, not here... */

          temp = "";
          if (lake_flag)
          {
            if (map_color_levels && scale_y > 128)
            {
              skip_label++;
            }
            if (mapshots_labels_flag && (fieldcount >= 4) )
            {
              temp = DBFReadStringAttribute( hDBF, structure, 3 );
            }
            else if (fieldcount >= 1)
            {
              temp = DBFReadStringAttribute( hDBF, structure, 0 );  // NAME (lakes)
            }
            else
            {
              temp = NULL;
            }
          }
          else if (city_flag)
          {
            if (map_color_levels && scale_y > 512)
            {
              skip_label++;
            }
            if (mapshots_labels_flag && (fieldcount >= 4) )
            {
              temp = DBFReadStringAttribute( hDBF, structure, 3 );  // NAME (designated places)
            }
            else
            {
              temp = NULL;
            }
          }
#endif /* !WITH_DBFAWK */
          /* XXX - figure out how to set this from dbfawk: */
          if (quad_overlay_flag)
          {
            if (fieldcount >= 5)
            {
              // Use just the last two characters of
              // the quad index.  "44072-A3" converts
              // to "A3"
              temp = DBFReadStringAttribute( hDBF, structure, 4 );
              xastir_snprintf(quad_label,
                              sizeof(quad_label),
                              "%s ",
                              &temp[strlen(temp) - 2]);

              // Append the name of the quad
              temp = DBFReadStringAttribute( hDBF, structure, 0 );
              strncat(quad_label,
                      temp,
                      sizeof(quad_label) - 1 - strlen(quad_label));
            }
            else
            {
              quad_label[0] = '\0';
            }
          }

          if ( (temp != NULL)
               && (strlen(temp) != 0)
               && map_labels
               && !skip_label )
          {
            int temp_ok;

            if (debug_level & 16)
            {
              fprintf(stderr,"I think I should display labels\n");
            }
            ok = 1;

            // Convert to Xastir coordinates:
            // If quad overlay shapefile, need to
            // snag the label coordinates from the DBF
            // file instead.  Note that the coordinates
            // are for the bottom right corner of the
            // quad, so we need to shift it left by 7.5'
            // to make the label appear inside the quad
            // (attached to the bottom left corner in
            // this case).
            /* XXX - figure out how to do WITH_DBFAWK: */
            if (quad_overlay_flag)
            {
              const char *dbf_temp;
              float lat_f;
              float lon_f;

              if (fieldcount >= 4)
              {
                dbf_temp = DBFReadStringAttribute( hDBF, structure, 2 );
                if (1 != sscanf(dbf_temp, "%f", &lat_f))
                {
                  fprintf(stderr,"draw_shapefile_map:sscanf parsing error\n");
                }
                dbf_temp = DBFReadStringAttribute( hDBF, structure, 3 );
                if (1 != sscanf(dbf_temp, "%f", &lon_f))
                {
                  fprintf(stderr,"draw_shapefile_map:sscanf parsing error\n");
                }
                lon_f = lon_f - 0.125;
              }
              else
              {
                lat_f = 0.0;
                lon_f = 0.0;
              }

              //fprintf(stderr,"Lat: %f, Lon: %f\t, Quad: %s\n", lat_f, lon_f, quad_label);

              temp_ok = convert_to_xastir_coordinates(&my_long,
                                                      &my_lat,
                                                      (float)lon_f,
                                                      (float)lat_f);
            }
            else    // Not quad overlay, use vertices
            {
              // XXX - todo: center large polygon labels in the center (e.g. county boundary)
              /* center label in polygon bounding box */
              temp_ok = convert_to_xastir_coordinates(&my_long,
                                                      &my_lat,
                                                      ((float)(object->dfXMax + object->dfXMin))/2.0,
                                                      ((float)(object->dfYMax + object->dfYMin))/2.0);
#ifdef notdef
              temp_ok = convert_to_xastir_coordinates(&my_long,
                                                      &my_lat,
                                                      (float)object->padfX[0],
                                                      (float)object->padfY[0]);
#endif  // notdef
            }
            //fprintf(stderr,"%ld %ld\n", my_long, my_lat);

            if (!temp_ok)
            {
              fprintf(stderr,"draw_shapefile_map8: Problem converting from lat/lon\n");
              ok = 0;
              x = 0;
              y = 0;
            }
            else
            {
              // Convert to screen coordinates.  Careful
              // here!  The format conversions you'll need
              // if you try to compress this into two
              // lines will get you into trouble.
              x = my_long - NW_corner_longitude;
              y = my_lat - NW_corner_latitude;
              x = x / scale_x;
              y = y / scale_y;

// Labeling of polygons done here

              if (ok == 1 && ok_to_draw)
              {
                if (quad_overlay_flag)
                {
                  draw_nice_string(w,
                                   pixmap,
                                   0,
                                   x+2,
                                   y-1,
                                   (char*)quad_label,
                                   0xf,
                                   0x10,
                                   strlen(quad_label));
                }
                else
                {
#ifdef notdef
                  (void)draw_label_text ( w,
                                          x,
                                          y,
                                          strlen(temp),
                                          colors[label_color],
                                          (char *)temp);
#endif  // notdef
                  if (debug_level & 16)
                  {
                    fprintf(stderr,
                            "  displaying label %s with color %x\n",
                            temp,label_color);
                  }
                  (void)draw_centered_label_text(w,
                                                 -90,
                                                 x,
                                                 y,
                                                 strlen(temp),
                                                 colors[label_color],
                                                 (char *)temp,
                                                 font_size);
                }
              }
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
      //fprintf(stderr,"free: %s\n",ptr2->label);
      free(ptr2);
      ptr2 = label_hash[i];
    }
  }

#ifdef WITH_DBFAWK
  dbfawk_free_info(fld_info);
  if (sig_info != NULL && sig_info != dbfawk_default_sig  && (sig_info->sig == NULL))
  {
    dbfawk_free_sigs(sig_info);
  }
#endif  // WITH_DBFAWK

  DBFClose( hDBF );
  SHPClose( hSHP );

//    XmUpdateDisplay (XtParent (da));

  if (debug_level & 16)
  {
    fprintf(stderr,"High-Mark Index:%d,\tHigh-Mark i:%d\n",
            high_water_mark_index,
            high_water_mark_i);
  }

  // Set fill style back to defaults
  XSetFillStyle(XtDisplay(w), gc, FillSolid);
}
// End of draw_shapefile_map()





#ifdef WITH_DBFAWK

// This function will delete any pre-loaded dbfawk sigs and clear Dbf_sigs
// This will trigger a  reload the first time a shapfile is redisplayed
void clear_dbfawk_sigs(void)
{
  //    fprintf(stderr,"Clearing signatures.\n");
  if (Dbf_sigs )
  {
    dbfawk_free_sigs(Dbf_sigs);
    Dbf_sigs = NULL;
  }
}

#endif  // WITH_DBFAWK


#endif  // HAVE_LIBSHP


