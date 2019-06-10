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

#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }

#define DOS_HDR_LINES 8
#define GRID_MORE 5000

extern int npoints;        /* tsk tsk tsk -- globals */
extern int mag;

#ifdef HAVE_LIBGEOTIFF


#include "xtiffio.h"
//#include "geotiffio.h"

#include "geo_normalize.h"

// Must be last include file
#include "leak_detection.h"





/**********************************************************
 * get_alt_fgd_path()
 *
 * Used to search for .fgd in ../metadata subdir, as it is
 * layed out on a USGS CDROM.
 **********************************************************/
void get_alt_fgd_path(char *fullpath, int fullpath_length)
{
  int len;
  int i, j = 0;
  char *dir = fullpath;
  char fname[MAX_FILENAME];

  // Split up into directory and filename
  len = (int)strlen (fullpath);
  for (i = len; i >= 0; i--)
  {
    if (fullpath[i] == '/')
    {
      dir = &fullpath[i];
      break;
    }
  }
  for (++i; i <= len; i++)
  {
    fname[j++] = fullpath[i];   // Grab the filename
    if (fullpath[i] == '\0')
    {
      break;
    }
  }

  // We have the filename now.  dir now points to
  // the '/' at the end of the path.

  // Now do it again to knock off the "data" subdirectory
  // from the end.
  dir[0] = '\0';  // Terminate the current string, wiping out the '/' character
  len = (int)strlen (fullpath);   // Length of the new shortened string
  for (i = len; i >= 0; i--)
  {
    if (fullpath[i] == '/')
    {
      dir = &fullpath[i + 1]; // Dir now points to one past the '/' character
      break;
    }
  }
  for (++i; i <= len; i++)
  {
    if (fullpath[i] == '\0')
    {
      break;
    }
  }

  // Add "metadata/" into the path
  xastir_snprintf(dir, fullpath_length, "metadata/%s", fname);
  //fprintf(stderr,"FGD Directory: %s\n", fullpath);
}





/**********************************************************
 * get_alt_fgd_path2()
 *
 * Used to search for .fgd in Metadata subdir.  This function
 * is no longer used.
 **********************************************************/
void get_alt_fgd_path2(char *fullpath, int fullpath_length)
{
  int len;
  int i, j = 0;
  char *dir = fullpath;
  char fname[MAX_FILENAME];

  // Split up into directory and filename
  len = (int)strlen (fullpath);
  for (i = len; i >= 0; i--)
  {
    if (fullpath[i] == '/')
    {
      dir = &fullpath[i + 1];
      break;
    }
  }
  for (++i; i <= len; i++)
  {
    fname[j++] = fullpath[i];
    if (fullpath[i] == '\0')
    {
      break;
    }
  }

  // Add "Metadata/" into the path
  xastir_snprintf(dir, fullpath_length, "Metadata/%s", fname);
}





/***********************************************************
 * read_fgd_file()
 *
 * Read in the "*.fgd" file associated with the geoTIFF
 * file.  Get the corner points from it and return.  If
 * no fgd file exists for this map, return a 0.
 ***********************************************************/
int read_fgd_file ( char* tif_filename,
                    float* f_west_bounding,
                    float* f_east_bounding,
                    float* f_north_bounding,
                    float* f_south_bounding)
{
  char fgd_file[MAX_FILENAME];/* Complete path/name of .fgd file */
  FILE *fgd;                  /* Filehandle of .fgd file */
  char line[MAX_FILENAME];    /* One line from .fgd file */
  int length;
  char *ptr;                  /* Substring pointer */
  int num_coordinates = 0;


  /* Read the .fgd file to find corners of the map neat-line */
  xastir_snprintf(fgd_file,
                  sizeof(fgd_file),
                  "%s",
                  tif_filename);
  length = strlen(fgd_file);

  /* Change the extension to ".fgd" */
  fgd_file[length-3] = 'f';
  fgd_file[length-2] = 'g';
  fgd_file[length-1] = 'd';

  if (debug_level & 512)
  {
    fprintf(stderr,"%s\n",fgd_file);
  }

  /*
   * Search for the WEST/EAST/NORTH/SOUTH BOUNDING COORDINATES
   * in the .fgd file.
   */
  fgd = fopen (fgd_file, "r");

  // Try an alternate path (../metadata/ subdirectory) if the first path didn't work
  // This allows working with USGS maps directly from CDROM
  if (fgd == NULL)
  {
    get_alt_fgd_path(fgd_file, sizeof(fgd_file) );

    if (debug_level & 512)
    {
      fprintf(stderr,"%s\n",fgd_file);
    }

    fgd = fopen (fgd_file, "r");
  }

  if (fgd != NULL)
  {
    while ( ( !feof (fgd) ) && ( num_coordinates < 4 ) )
    {
      get_line (fgd, line, MAX_FILENAME);

      if (*f_west_bounding == 0.0)
      {
        if ( ( (ptr = strstr(line, "WEST BOUNDING COORDINATE:") ) != NULL)
             || ( (ptr = strstr(line, "West_Bounding_Coordinate:") ) != NULL) )
        {
          if (1 != sscanf (ptr + 25, " %f", f_west_bounding))
          {
            fprintf(stderr,"read_fgd_file:sscanf parsing error\n");
          }
          if (debug_level & 512)
          {
            fprintf(stderr,"West Bounding:  %f\n",*f_west_bounding);
          }
          num_coordinates++;
        }
      }

      else if (*f_east_bounding == 0.0)
      {
        if ( ( (ptr = strstr(line, "EAST BOUNDING COORDINATE:") ) != NULL)
             || ( (ptr = strstr(line, "East_Bounding_Coordinate:") ) != NULL) )
        {
          if (1 != sscanf (ptr + 25, " %f", f_east_bounding))
          {
            fprintf(stderr,"read_fgd_file:sscanf parsing error\n");
          }
          if (debug_level & 512)
          {
            fprintf(stderr,"East Bounding:  %f\n",*f_east_bounding);
          }
          num_coordinates++;
        }
      }

      else if (*f_north_bounding == 0.0)
      {
        if ( ( (ptr = strstr(line, "NORTH BOUNDING COORDINATE:") ) != NULL)
             || ( (ptr = strstr(line, "North_Bounding_Coordinate:") ) != NULL) )
        {
          if (1 != sscanf (ptr + 26, " %f", f_north_bounding))
          {
            fprintf(stderr,"read_fgd_file:sscanf parsing error\n");
          }
          if (debug_level & 512)
          {
            fprintf(stderr,"North Bounding: %f\n",*f_north_bounding);
          }
          num_coordinates++;
        }
      }

      else if (*f_south_bounding == 0.0)
      {
        if ( ( (ptr = strstr(line, "SOUTH BOUNDING COORDINATE:") ) != NULL)
             || ( (ptr = strstr(line, "South_Bounding_Coordinate:") ) != NULL) )
        {
          if (1 != sscanf (ptr + 26, " %f", f_south_bounding))
          {
            fprintf(stderr,"read_fgd_file:sscanf parsing error\n");
          }
          if (debug_level & 512)
          {
            fprintf(stderr,"South Bounding: %f\n",*f_south_bounding);
          }
          num_coordinates++;
        }
      }

    }
    fclose (fgd);
  }
  else
  {
    if (debug_level & 512)
      fprintf(stderr,"Couldn't open '.fgd' file, assuming no map collar to chop %s\n",
              tif_filename);
    return(0);
  }


  /*
   * We should now have exactly four bounding coordinates.
   * These specify the map neat-line corners.  We can use
   * them to chop off the white collar from around the map.
   */
  if (num_coordinates != 4)
  {
    fprintf(stderr,"Couldn't find 4 bounding coordinates in '.fgd' file, map %s\n",
            tif_filename);
    return(0);
  }


  if (debug_level & 512)
  {
    fprintf(stderr,"%f %f %f %f\n",
            *f_south_bounding,
            *f_north_bounding,
            *f_west_bounding,
            *f_east_bounding);
  }

  return(1);    /* Successful */
}

/***********************************************************
 * draw_geotiff_image_map()
 *
 * Here's where we handle geoTIFF files, such as USGS DRG
 * topo maps.  The .fgd file gives us the lat/lon of the map
 * neat-line corners for USGS maps.  We use this info to
 * chop off the white map border.  If no .fgd file is present,
 * we assume there is no map collar to be cropped and display
 * every pixel.
 * We also translate from the map datum to WGS84.  We use
 * libgeotiff/libtiff/libproj for these operations.

 * TODO:
 * Provide support for datums other than NAD27/NAD83/WGS84.
 * Libproj doesn't currently support many datums.
 *
 * Provide support for handling different map projections.
 * Perhaps by reprojecting the map data and storing it on
 * disk in another format.
 *
 * Select 'o', 'f', 'k', or 'c' maps based on zoom level.
 * Might also put some hysteresis in this so that it keeps
 * the current type of map through one extra zoom each way.
 * 'c': Good from x256 to x064.
 * 'f': Good from x128 to x032.  Not very readable at x128.
 * 'k': Good from x??? to x???.
 * 'o': Good from x064 to x004.  Not very readable at x64.
 ***********************************************************/
void draw_geotiff_image_map (Widget w,
                             char *dir,
                             char *filenm,
                             alert_entry * UNUSED(alert),
                             u_char UNUSED(alert_color),
                             int destination_pixmap,
                             map_draw_flags *mdf)
{
  char file[MAX_FILENAME];    /* Complete path/name of image file */
  char short_filenm[MAX_FILENAME];
  TIFF *tif = (TIFF *) 0;     /* Filehandle for tiff image file */
  GTIF *gtif = (GTIF *) 0;    /* GeoKey-level descriptor */
  /* enum { VERSION = 0, MAJOR, MINOR }; */
  int versions[3];
  uint32 width;               /* Width of the image */
  uint32 height;              /* Height of the image */
  uint16 bitsPerSample;       /* Should be 8 for USGS DRG's */
  uint16 samplesPerPixel = 1; /* Should be 1 for USGS DRG's.  Some maps
                                   don't have this tag so we default to 1 */
  uint32 rowsPerStrip;        /* Should be 1 for USGS DRG's */
  uint16 planarConfig;        /* Should be 1 for USGS DRG's */
  uint16 photometric;         /* DRGs are RGB (2) */
  int    bytesPerRow;            /* Bytes per scanline row of tiff file */
  GTIFDefn defn;              /* Stores geotiff details */
  u_char *imageMemory;        /* Fixed pointer to same memory area */
  uint32 row;                 /* My row counter for the loop */
  int num_colors;             /* Number of colors in the geotiff colormap */
  uint16 *red_orig, *green_orig, *blue_orig; /* Used for storing geotiff colors */
  XColor my_colors[256];      /* Used for translating colormaps */
  unsigned long west_bounding = 0;
  unsigned long east_bounding = 0;
  unsigned long north_bounding = 0;
  unsigned long south_bounding = 0;
  float f_west_bounding = 0.0;
  float f_east_bounding = 0.0;
  float f_north_bounding = 0.0;
  float f_south_bounding = 0.0;

  unsigned long west_bounding_wgs84 = 0;
  unsigned long east_bounding_wgs84 = 0;
  unsigned long north_bounding_wgs84 = 0;
  unsigned long south_bounding_wgs84 = 0;

  float f_NW_x_bounding;
  float f_NW_y_bounding;
  float f_NE_x_bounding;
  float f_NE_y_bounding;
  float f_SW_x_bounding;
  float f_SW_y_bounding;
  float f_SE_x_bounding;
  float f_SE_y_bounding;

  unsigned long NW_x_bounding_wgs84 = 0;
  unsigned long NW_y_bounding_wgs84 = 0;
  double f_NW_x_bounding_wgs84 = 0.0;
  double f_NW_y_bounding_wgs84 = 0.0;

  unsigned long NE_x_bounding_wgs84 = 0;
  unsigned long NE_y_bounding_wgs84 = 0;
  double f_NE_x_bounding_wgs84 = 0.0;
  double f_NE_y_bounding_wgs84 = 0.0;

  unsigned long SW_x_bounding_wgs84 = 0;
  unsigned long SW_y_bounding_wgs84 = 0;
  double f_SW_x_bounding_wgs84 = 0.0;
  double f_SW_y_bounding_wgs84 = 0.0;

  unsigned long SE_x_bounding_wgs84 = 0;
  unsigned long SE_y_bounding_wgs84 = 0;
  double f_SE_x_bounding_wgs84 = 0.0;
  double f_SE_y_bounding_wgs84 = 0.0;

  int NW_x = 0;               /* Store pixel values for map neat-line */
  int NW_y = 0;               /* ditto */
  int NE_x = 0;               /* ditto */
  int NE_y = 0;               /* ditto */
  int SW_x = 0;               /* ditto */
  int SW_y = 0;               /* ditto */
  int SE_x = 0;               /* ditto */
  int SE_y = 0;               /* ditto */
  int left_crop;              /* Pixel cropping value */
  int right_crop;             /* Pixel cropping value */
  int top_crop;               /* Pixel cropping value */
  int bottom_crop;            /* Pixel cropping value */
  double xxx, yyy;            /* LFM: needs more accuracy here */
  long sxx, syy;              /* X Y screen plot positions          */
  float steph;
  float stepw;
  int stepwc, stephc;
  char map_it[MAX_FILENAME];           /* Used to hold filename for status line */
  int have_fgd;               /* Tells where we have an associated *.fgd file */
  //short datum;
  char *datum_name;           /* Points to text name of datum */
  //double *GeoTie;
  int crop_it = 0;            /* Flag which tells whether the image should be cropped */

  uint32 column;

  float xastir_left_x_increment;
  float left_x_increment;
  float xastir_left_y_increment;
  float left_y_increment;
  float xastir_right_x_increment;
  float right_x_increment;
  float xastir_right_y_increment;
  float right_y_increment;
  float xastir_top_y_increment;
  float top_y_increment;
  float xastir_bottom_y_increment;
  float bottom_y_increment;
  //    float xastir_avg_y_increment;
  float avg_y_increment;
  int row_offset;
  unsigned long current_xastir_left;
  unsigned long current_xastir_right;
  uint32 current_left;
  uint32 current_right;
  //    uint32 current_line_width;
  unsigned long xastir_current_y;
  uint32 column_offset;
  unsigned long xastir_current_x;
  double *PixelScale;
  int have_PixelScale;
  uint16 qty;
  int SkipRows;
  unsigned long view_min_x, view_max_x;
  unsigned long view_min_y, view_max_y;

  unsigned long xastir_total_y;
  int NW_line_offset;
  int NE_line_offset;
  int NW_xastir_x_offset;
  int NE_xastir_x_offset;
  int NW_xastir_y_offset;
  int NW_x_offset;
  int NE_x_offset;
  float xastir_avg_left_right_y_increment;
  float total_avg_y_increment;
  unsigned long view_left_minus_pixel_width;
  unsigned long view_top_minus_pixel_height;
  int proj_is_latlong;
  short PCS;
  int usgs_drg;
  char *imagedesc;

  usgs_drg = mdf->usgs_drg;              // yes, no, or auto

  if (debug_level & 16)
  {
    fprintf(stderr,"%s/%s\n", dir, filenm);
  }


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

  /* Check whether we have an associated *.fgd file.  This
   * file contains the neat-line corner points for USGS DRG
   * maps, which allows us to chop off the map collar.
   */
  have_fgd = read_fgd_file( file,
                            &f_west_bounding,
                            &f_east_bounding,
                            &f_north_bounding,
                            &f_south_bounding );

  /*
   * If we are able to read the fgd file then we have the lat/lon
   * corner points in floating point variables.  If there isn't
   * an fgd file then we must get the info from the geotiff
   * tags themselves and we assume that there's no map collar to
   * chop off.
   */


  /*
   * What we NEED to do (implemented a bit later in this function
   * in order to support geotiff files created with other map
   * datums, is to open up the geotiff file and get the map datum
   * used for the data.  Then convert the corner points to WGS84
   * and check to see whether the image is inside our viewport.
   * Some USGS geotiff maps have map data in NAD83 datum and the
   * .fgd file incorrectly specifying NAD27 datum.  There are also
   * some USGS geotiff maps created with WGS84 datum.
   */


  /* convert_to_xastir_coordinates( x,y,longitude,latitude ); */
  if (have_fgd)   /* Could be a USGS file */
  {
    int temp_ok1, temp_ok2;

    if (debug_level & 16)
    {
      fprintf(stderr,"FGD:  W:%f  E:%f  N:%f  S:%f\n",
              f_west_bounding,
              f_east_bounding,
              f_north_bounding,
              f_south_bounding);
    }

    crop_it = 1;        /* The map collar needs to be cropped */

    temp_ok1 = convert_to_xastir_coordinates(  &west_bounding,
               &north_bounding,
               f_west_bounding,
               f_north_bounding );


    temp_ok2 = convert_to_xastir_coordinates(  &east_bounding,
               &south_bounding,
               f_east_bounding,
               f_south_bounding );

    if (!temp_ok1 || !temp_ok2)
    {
      fprintf(stderr,"draw_geotiff_image_map: problem converting from lat/lon\n");
      return;
    }


    /*
     * Check whether map is inside our current view.  It'd be
     * good to do a datum conversion first, but we don't know
     * what the datum is by this point in the code.  I'm just
     * doing this check here for speed, so that I can eliminate
     * maps that aren't even close to our viewport area, without
     * having to open those map files.  All other maps that pass
     * this test (at the next go-around later in the code) must
     * have their corner points datum-shifted so that we can
     * REALLY tell whether a map fits within the viewport.
     *
     * Perhaps add a bit to the corners (the max datum shift?)
     * to do our quick check?  I decided to add about 10 seconds
     * to the map edges, which equates to 1000 in the Xastir
     * coordinate system.  That should be greater than any datum
     * shift in North America for USGS topos.  I'm artificially
     * inflating the size of the map just for this quick
     * elimination check.
     *
     *   bottom          top             left           right
     */

    // Check whether we're indexing or drawing the map
    if ( (destination_pixmap != INDEX_CHECK_TIMESTAMPS)
         && (destination_pixmap != INDEX_NO_TIMESTAMPS) )
    {

      // We're drawing.
      if (!map_visible( south_bounding + 1000,
                        north_bounding - 1000,
                        west_bounding - 1000,
                        east_bounding + 1000 ) )
      {
        if (debug_level & 16)
        {
          fprintf(stderr,"Map not within current view.\n");
          fprintf(stderr,"Skipping map: %s\n", file);
        }

        // Map isn't inside our current view.  We're done.
        // Free any memory used and return.
        //
        return;    // Skip this map
      }
      else
      {
        if (debug_level & 16)
        {
          fprintf(stderr,"Map is viewable\n");
        }
      }
    }
  }   // End of if have_fgd


  /*
   * At this point the map MAY BE in our current view.
   * We don't know for sure until we do a datum translation
   * on the bounding coordinates and check again.  Note that
   * if there's not an accompanying .fgd file, we don't have
   * the bounding coordinates yet by this point.
   */

  if (debug_level & 16)
  {
    fprintf(stderr,"XTIFFOpen\n");
  }

  /* Open TIFF descriptor to read GeoTIFF tags */
  tif = XTIFFOpen (file, "r");
  if (!tif)
  {
    return;
  }

  if (debug_level & 16)
  {
    fprintf(stderr,"GTIFNew\n");
  }

  /* Open GTIF Key parser.  Keys will be read at this time */
  gtif = GTIFNew (tif);
  if (!gtif)
  {
    /* Close the TIFF file descriptor */
    XTIFFClose (tif);
    return;
  }

  if (debug_level & 16)
  {
    fprintf(stderr,"GTIFDirectoryInfo\n");
  }

  /*
   * Get the GeoTIFF directory info.  Need this for
   * some of the operations further down in the code.
   */
  GTIFDirectoryInfo (gtif, versions, 0);

  /*
  if (versions[MAJOR] > 1)
  {
      fprintf(stderr,"This file is too new for me\n");
      GTIFFree (gtif);
      XTIFFClose (tif);
      return;
  }
  */

  if (debug_level & 16)
  {
    fprintf(stderr,"GTIFGetDefn\n");
  }


  /* I might want to attempt to avoid the GTIFGetDefn
   * call, as it takes a bit of time per file.  It
   * normalizes the info.  Try getting just the tags
   * or keys that I need individually instead.  I
   * need "defn" for the GTIFProj4ToLatLong calls though.
   */
  if (GTIFGetDefn (gtif, &defn))
  {
    if (debug_level & 16)
    {
      GTIFPrintDefn (&defn, stdout);
    }
  }
  else
  {
    fprintf(stderr,"GTIFGetDefn failed\n");
  }

  proj_is_latlong=FALSE;

  if( !GTIFKeyGet(gtif,ProjectedCSTypeGeoKey, &PCS,0,1))
  {
    // fprintf(stderr,"Warning: no PCS in geotiff file %s, assuming map is in lat/lon!\n", filenm);
    proj_is_latlong=TRUE;
  }

  /* Fetch a few TIFF fields for this image */
  if ( !TIFFGetField (tif, TIFFTAG_IMAGEWIDTH, &width) )
  {
    width = 5493;
    fprintf(stderr,"No width tag found in file, setting it to 5493\n");
  }

  if ( !TIFFGetField (tif, TIFFTAG_IMAGELENGTH, &height) )
  {
    height = 6840;
    fprintf(stderr,"No height tag found in file, setting it to 6840\n");
  }

  // If we're autodetecting usgs_drg, check the image description tag
  // Note, the TIFFGetField doesn't allocate a string, it returns a pointer
  // to an existing one.  Don't free it!
  if (usgs_drg == 2)
  {
    if ( TIFFGetField (tif, TIFFTAG_IMAGEDESCRIPTION, &imagedesc))
    {
      if (strncasecmp(imagedesc,"USGS GeoTIFF DRG",16) == 0)
      {
        usgs_drg = 1;   // Yes
      }
      else
      {
        usgs_drg = 0;  // No
      }
    }
    else
    {
      usgs_drg = 0;  // No tag, assume not a usgs topo
    }

  }

  /*
   * If we don't have an associated .fgd file for this map,
   * check for corner points in the ImageDescription
   * tag (proposed new USGS DRG standard).  Boundary
   * coordinates will be the outside corners of the image
   * unless I can find some other proof.
   * Currently I assume that the map has no
   * map collar to chop off and set the neat-line corners
   * to be the outside corners of the image.
   *
   * NOTE:  For the USGS files (with a map collar), the
   * image must be cropped and rotated and is slightly
   * narrower at one end (top for northern hemisphere, bottom
   * for southern hemisphere).  For other files with no map
   * collar, the image is rectangular but the lat/lon
   * coordinates may be rotated.
   */
  if (!have_fgd)      // Not a USGS map or perhaps a newer spec
  {
    crop_it = 1;        /* crop this map image */

    /*
     * Snag and parse ImageDescription tag here.
     */

    /* Code goes here for getting ImageDescription tag... */


    /* Figure out the bounding coordinates for this map */
    if (debug_level & 16)
    {
      fprintf(stderr,"\nCorner Coordinates:\n");
    }

    /* Find lat/lon for NW corner of image */
    xxx = 0.0;
    yyy = 0.0;
    if ( GTIFImageToPCS( gtif, &xxx, &yyy ) )   // Do all 4 of these in one call?
    {
      if (debug_level & 16)
      {
        fprintf(stderr,"%-13s ", "Upper Left" );
        fprintf(stderr,"(%11.3f,%11.3f)\n", xxx, yyy );
      }
    }

    if ( proj_is_latlong || GTIFProj4ToLatLong( &defn, 1, &xxx, &yyy ) )   // Do all 4 of these in one call?
    {
      if (debug_level & 16)
      {
        fprintf(stderr,"  (%s,", GTIFDecToDMS( xxx, "Long", 2 ) );
        fprintf(stderr,"%s)\n", GTIFDecToDMS( yyy, "Lat", 2 ) );
        fprintf(stderr,"%f  %f\n", xxx, yyy);
      }
    }
    else
    {
      if (!proj_is_latlong)
      {
        fprintf(stderr,"Failed GTIFProj4ToLatLong() call\n");
      }
    }

    f_NW_x_bounding = (float)xxx;
    f_NW_y_bounding = (float)yyy;


    /* Find lat/lon for NE corner of image */
    xxx = width - 1;
    yyy = 0.0;
    if ( GTIFImageToPCS( gtif, &xxx, &yyy ) )
    {
      if (debug_level & 16)
      {
        fprintf(stderr,"%-13s ", "Lower Right" );
        fprintf(stderr,"(%11.3f,%11.3f)\n", xxx, yyy );
      }
    }

    if (  proj_is_latlong || GTIFProj4ToLatLong( &defn, 1, &xxx, &yyy ) )
    {
      if (debug_level & 16)
      {
        fprintf(stderr,"  (%s,", GTIFDecToDMS( xxx, "Long", 2 ) );
        fprintf(stderr,"%s)\n", GTIFDecToDMS( yyy, "Lat", 2 ) );
        fprintf(stderr,"%f  %f\n", xxx, yyy);
      }
    }
    else
    {
      if (!proj_is_latlong)
      {
        fprintf(stderr,"Failed GTIFProj4ToLatLong() call\n");
      }
    }

    f_NE_x_bounding = (float)xxx;
    f_NE_y_bounding = (float)yyy;

    /* Find lat/lon for SW corner of image */
    xxx = 0.0;
    yyy = height - 1;
    if ( GTIFImageToPCS( gtif, &xxx, &yyy ) )
    {
      if (debug_level & 16)
      {
        fprintf(stderr,"%-13s ", "Lower Right" );
        fprintf(stderr,"(%11.3f,%11.3f)\n", xxx, yyy );
      }
    }

    if (  proj_is_latlong || GTIFProj4ToLatLong( &defn, 1, &xxx, &yyy ) )
    {
      if (debug_level & 16)
      {
        fprintf(stderr,"  (%s,", GTIFDecToDMS( xxx, "Long", 2 ) );
        fprintf(stderr,"%s)\n", GTIFDecToDMS( yyy, "Lat", 2 ) );
        fprintf(stderr,"%f  %f\n", xxx, yyy);
      }
    }
    else
    {
      if (!proj_is_latlong)
      {
        fprintf(stderr,"Failed GTIFProj4ToLatLong() call\n");
      }
    }

    f_SW_x_bounding = (float)xxx;
    f_SW_y_bounding = (float)yyy;

    /* Find lat/lon for SE corner of image */
    xxx = width - 1;
    yyy = height - 1;
    if ( GTIFImageToPCS( gtif, &xxx, &yyy ) )
    {
      if (debug_level & 16)
      {
        fprintf(stderr,"%-13s ", "Lower Right" );
        fprintf(stderr,"(%11.3f,%11.3f)\n", xxx, yyy );
      }
    }

    if (  proj_is_latlong || GTIFProj4ToLatLong( &defn, 1, &xxx, &yyy ) )
    {
      if (debug_level & 16)
      {
        fprintf(stderr,"  (%s,", GTIFDecToDMS( xxx, "Long", 2 ) );
        fprintf(stderr,"%s)\n", GTIFDecToDMS( yyy, "Lat", 2 ) );
        fprintf(stderr,"%f  %f\n", xxx, yyy);
      }
    }
    else
    {
      if (!proj_is_latlong)
      {
        fprintf(stderr,"Failed GTIFProj4ToLatLong() call\n");
      }
    }

    f_SE_x_bounding = (float)xxx;
    f_SE_y_bounding = (float)yyy;

    if (f_NW_y_bounding > 0)
    {
      yyy=((f_NW_y_bounding > f_NE_y_bounding) ? f_NE_y_bounding
           : f_NW_y_bounding);
      xxx=((f_SW_y_bounding < f_SE_y_bounding) ? f_SE_y_bounding
           : f_SW_y_bounding);
    }
    else
    {
      yyy=((f_NW_y_bounding < f_NE_y_bounding) ? f_NE_y_bounding
           : f_NW_y_bounding);
      xxx=((f_SW_y_bounding > f_SE_y_bounding) ? f_SE_y_bounding
           : f_SW_y_bounding);
    }
    f_north_bounding = (float)yyy;
    f_south_bounding = (float)xxx;
    if (f_NE_x_bounding > 0)
    {
      xxx=((f_NE_x_bounding < f_SE_x_bounding) ? f_SE_x_bounding
           : f_NE_x_bounding);
      yyy=((f_NW_x_bounding > f_SW_x_bounding) ? f_SW_x_bounding
           : f_NW_x_bounding);
    }
    else
    {
      xxx=((f_NE_x_bounding > f_SE_x_bounding) ? f_SE_x_bounding
           : f_NE_x_bounding);
      yyy=((f_NW_x_bounding < f_SW_x_bounding) ? f_SW_x_bounding
           : f_NW_x_bounding);
    }
    f_west_bounding = (float)yyy;
    f_east_bounding = (float)xxx;
  }

  f_NW_x_bounding = f_west_bounding;
  f_NW_y_bounding = f_north_bounding;

  f_SW_x_bounding = f_west_bounding;
  f_SW_y_bounding = f_south_bounding;

  f_NE_x_bounding = f_east_bounding;
  f_NE_y_bounding = f_north_bounding;

  f_SE_x_bounding = f_east_bounding;
  f_SE_y_bounding = f_south_bounding;


  // Fill in the wgs84 variables so we can do a datum
  // conversion but keep our original values also.
  f_NW_x_bounding_wgs84 = f_NW_x_bounding;
  f_NW_y_bounding_wgs84 = f_NW_y_bounding;

  f_SW_x_bounding_wgs84 = f_SW_x_bounding;
  f_SW_y_bounding_wgs84 = f_SW_y_bounding;

  f_NE_x_bounding_wgs84 = f_NE_x_bounding;
  f_NE_y_bounding_wgs84 = f_NE_y_bounding;

  f_SE_x_bounding_wgs84 = f_SE_x_bounding;
  f_SE_y_bounding_wgs84 = f_SE_y_bounding;


  /* Get the datum */
  // GTIFKeyGet( gtif, GeogGeodeticDatumGeoKey, &datum, 0, 1 );
  // if (debug_level & 16)
  //     fprintf(stderr,"GeogGeodeticDatumGeoKey: %d\n", datum );


  /* Get the tiepoints (in UTM coordinates always?)
   * In our case they look like:
   *
   * 0.000000         Y
   * 0.000000         X
   * 0.000000         Z
   * 572983.025771    Y in UTM (longitude for some maps?)
   * 5331394.085064   X in UTM (latitude for some maps?)
   * 0.000000         Z
   *
   */
  /*
  if (debug_level & 16) {
      fprintf(stderr,"Tiepoints:\n");
      if ( TIFFGetField( tif, TIFFTAG_GEOTIEPOINTS, &qty, &GeoTie ) ) {
          for ( i = 0; i < qty; i++ ) {
              fprintf(stderr,"%f\n", *(GeoTie + i) );
          }
      }
  }
  */


  /* Get the geotiff horizontal datum name */
  if ( defn.Datum != 32767 )
  {
    GTIFGetDatumInfo( defn.Datum, &datum_name, NULL );
    if (debug_level & 16)
    {
      fprintf(stderr,"Datum: %d/%s\n", defn.Datum, datum_name );
    }
  }


  /*
   * Perform a datum shift on the bounding coordinates before we
   * check whether the map is inside our viewport.  At the moment
   * this is still hard-coded to NAD27 datum.  If the map is already
   * in WGS84 or NAD83 datum, skip the datum conversion code.
   */
  if (   (defn.Datum != 6030)     /* DatumE_WGS84 */
         && (defn.Datum != 6326)     /*  Datum_WGS84 */
         && (defn.Datum != 6269) )   /* Datum_North_American_Datum_1983 */
  {

    if (debug_level & 16)
    {
      fprintf(stderr,"***** Attempting Datum Conversions\n");
    }


    // This code uses datum.h/datum.c to do the conversion
    // instead of the proj.4 library as we had before.
    // Here we assume that if it's not one of the three datums
    // listed above, it's NAD27.

    // Convert NW corner to WGS84
    wgs84_datum_shift(TO_WGS_84,
                      &f_NW_y_bounding_wgs84,
                      &f_NW_x_bounding_wgs84,
                      D_NAD_27_CONUS);   // NAD27 CONUS

    // Convert NE corner to WGS84
    wgs84_datum_shift(TO_WGS_84,
                      &f_NE_y_bounding_wgs84,
                      &f_NE_x_bounding_wgs84,
                      D_NAD_27_CONUS);   // NAD27 CONUS

    // Convert SW corner to WGS84
    wgs84_datum_shift(TO_WGS_84,
                      &f_SW_y_bounding_wgs84,
                      &f_SW_x_bounding_wgs84,
                      D_NAD_27_CONUS);   // NAD27 CONUS

    // Convert SE corner to WGS84
    wgs84_datum_shift(TO_WGS_84,
                      &f_SE_y_bounding_wgs84,
                      &f_SE_x_bounding_wgs84,
                      D_NAD_27_CONUS);   // NAD27 CONUS (131)
  }
  else if (debug_level & 16)
  {
    fprintf(stderr,"***** Skipping Datum Conversion\n");
  }


  /*
   * Convert new datum-translated bounding coordinates to the
   * Xastir coordinate system.
   * convert_to_xastir_coordinates( x,y,longitude,latitude )
   */
  // NW corner
  if (!convert_to_xastir_coordinates(  &NW_x_bounding_wgs84,
                                       &NW_y_bounding_wgs84,
                                       (float)f_NW_x_bounding_wgs84,
                                       (float)f_NW_y_bounding_wgs84 ) )
  {
    fprintf(stderr,"draw_geotiff_image_map: Problem converting from lat/lon\n");
    fprintf(stderr,"Did you follow the instructions for installing PROJ?\n");
    return;
  }

  // NE corner
  if (!convert_to_xastir_coordinates(  &NE_x_bounding_wgs84,
                                       &NE_y_bounding_wgs84,
                                       (float)f_NE_x_bounding_wgs84,
                                       (float)f_NE_y_bounding_wgs84 ) )
  {
    fprintf(stderr,"draw_geotiff_image_map: Problem converting from lat/lon\n");
    fprintf(stderr,"Did you follow the instructions for installing PROJ?\n");

    return;
  }

  // SW corner
  if (!convert_to_xastir_coordinates(  &SW_x_bounding_wgs84,
                                       &SW_y_bounding_wgs84,
                                       (float)f_SW_x_bounding_wgs84,
                                       (float)f_SW_y_bounding_wgs84 ) )
  {
    fprintf(stderr,"draw_geotiff_image_map: Problem converting from lat/lon\n");
    fprintf(stderr,"Did you follow the instructions for installing PROJ?\n");

    return;
  }

  // SE corner
  if (!convert_to_xastir_coordinates(  &SE_x_bounding_wgs84,
                                       &SE_y_bounding_wgs84,
                                       (float)f_SE_x_bounding_wgs84,
                                       (float)f_SE_y_bounding_wgs84 ) )
  {
    fprintf(stderr,"draw_geotiff_image_map: Problem converting from lat/lon\n");
    fprintf(stderr,"Did you follow the instructions for installing PROJ?\n");

    return;
  }


  /*
   * Check whether map is inside our current view.  These
   * are the real datum-shifted bounding coordinates now,
   * so this is the final decision as to whether the map
   * should be loaded.
   */

  // Find the largest dimensions
  if (NW_y_bounding_wgs84 <= NE_y_bounding_wgs84)
  {
    north_bounding_wgs84 = NW_y_bounding_wgs84;
  }
  else
  {
    north_bounding_wgs84 = NE_y_bounding_wgs84;
  }

  if (NW_x_bounding_wgs84 <= SW_x_bounding_wgs84)
  {
    west_bounding_wgs84 = NW_x_bounding_wgs84;
  }
  else
  {
    west_bounding_wgs84 = SW_x_bounding_wgs84;
  }

  if (SW_y_bounding_wgs84 >= SE_y_bounding_wgs84)
  {
    south_bounding_wgs84 = SW_y_bounding_wgs84;
  }
  else
  {
    south_bounding_wgs84 = SE_y_bounding_wgs84;
  }

  if (NE_x_bounding_wgs84 >= SE_x_bounding_wgs84)
  {
    east_bounding_wgs84 = NE_x_bounding_wgs84;
  }
  else
  {
    east_bounding_wgs84 = SE_x_bounding_wgs84;
  }


  // Check whether we're indexing or drawing the map
  if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
       || (destination_pixmap == INDEX_NO_TIMESTAMPS) )
  {

    xastir_snprintf(map_it,
                    sizeof(map_it),
                    langcode ("BBARSTA039"),
                    short_filenm);
    statusline(map_it,0);       // Indexing ...

    // We're indexing only.  Save the extents in the index.
    index_update_xastir(filenm, // Filename only
                        south_bounding_wgs84,   // Bottom
                        north_bounding_wgs84,   // Top
                        west_bounding_wgs84,    // Left
                        east_bounding_wgs84,    // Right
                        0);                     // Default Map Level

    //Free any memory used and return
    /* We're finished with the geoTIFF key parser, so get rid of it */
    GTIFFree (gtif);

    /* Close the TIFF file descriptor */
    XTIFFClose (tif);

    return; // Done indexing this file
  }
  else
  {
    xastir_snprintf(map_it,
                    sizeof(map_it),
                    langcode ("BBARSTA028"),
                    short_filenm);
    statusline(map_it,0);       // Loading ...
  }




  // bottom top left right
  if (!map_visible( south_bounding_wgs84,
                    north_bounding_wgs84,
                    west_bounding_wgs84,
                    east_bounding_wgs84 ) )
  {
    if (debug_level & 16)
    {
      fprintf(stderr,"Map not within current view.\n");
      fprintf(stderr,"Skipping map: %s\n", file);
    }

    /*
     * Map isn't inside our current view.  We're done.
     * Free any memory used and return
     */

    /* We're finished with the geoTIFF key parser, so get rid of it */
    GTIFFree (gtif);

    /* Close the TIFF file descriptor */
    XTIFFClose (tif);

    return;         /* Skip this map */
  }


  HandlePendingEvents(app_context);
  if (interrupt_drawing_now)
  {
    GTIFFree (gtif);
    XTIFFClose (tif);
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


  /*
  From running in debug mode:
              Width: 5493
             Height: 6840
     Rows Per Strip: 1
    Bits Per Sample: 8
  Samples Per Pixel: 1
      Planar Config: 1
  */


  /* Fetch a few TIFF fields for this image */
  if ( !TIFFGetField (tif, TIFFTAG_PHOTOMETRIC, &photometric) )
  {
    photometric = PHOTOMETRIC_RGB;
    fprintf(stderr,"No photometric tag found in file, setting it to RGB\n");
  }
  if ( !TIFFGetField (tif, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip) )
  {
    rowsPerStrip = 1;
    fprintf(stderr,"No rowsPerStrip tag found in file, setting it to 1\n");
  }

  if ( !TIFFGetField (tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample) )
  {
    bitsPerSample = 8;
    fprintf(stderr,"No bitsPerSample tag found in file, setting it to 8\n");
  }

  if ( !TIFFGetField (tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel) )
  {
    samplesPerPixel = 1;
    fprintf(stderr,"No samplesPerPixel tag found in file, setting it to 1\n");
  }

  if ( !TIFFGetField (tif, TIFFTAG_PLANARCONFIG, &planarConfig) )
  {
    planarConfig = 1;
    fprintf(stderr,"No planarConfig tag found in file, setting it to 1\n");
  }


  if (debug_level & 16)
  {
    fprintf(stderr,"            Width: %ld\n", (long int)width);
    fprintf(stderr,"           Height: %ld\n", (long int)height);
    fprintf(stderr,"      Photometric: %d\n", photometric);
    fprintf(stderr,"   Rows Per Strip: %ld\n", (long int)rowsPerStrip);
    fprintf(stderr,"  Bits Per Sample: %d\n", bitsPerSample);
    fprintf(stderr,"Samples Per Pixel: %d\n", samplesPerPixel);
    fprintf(stderr,"    Planar Config: %d\n", planarConfig);
  }


  /*
   * Check for properly formatted geoTIFF file.  If it isn't
   * in the standard format we're looking for, spit out an
   * error message and return.
   *
   * Should we also check compression method here?
   */
  /* if ( (   rowsPerStrip != 1) */
  if ( (samplesPerPixel != 1)
       || (  bitsPerSample != 8)
       || (   planarConfig != 1) )
  {
    fprintf(stderr,"*** geoTIFF file %s is not in the proper format:\n", file);
    if (samplesPerPixel != 1)
      fprintf(stderr,"***** Has %d samples per pixel instead of 1\n",
              samplesPerPixel);
    if (bitsPerSample != 8)
      fprintf(stderr,"***** Has %d bits per sample instead of 8\n",
              bitsPerSample);
    if (planarConfig != 1)
      fprintf(stderr,"***** Has planarConfig of %d instead of 1\n",
              planarConfig);
    fprintf(stderr,"*** Please reformat it and try again.\n");
    XTIFFClose(tif);
    return;
  }


  if (debug_level & 16)
  {
    fprintf(stderr,"Loading geoTIFF map: %s\n", file);
  }


  /*
   * Snag the original map colors out of the colormap embedded
   * inside the tiff file.
   */
  if (photometric == PHOTOMETRIC_PALETTE)
  {
    if (!TIFFGetField(tif, TIFFTAG_COLORMAP, &red_orig, &green_orig, &blue_orig))
    {
      TIFFError(TIFFFileName(tif), "Missing required \"Colormap\" tag");
      GTIFFree (gtif);
      XTIFFClose (tif);
      return;
    }
  }

  /* Here are the number of possible colors.  It turns out to
   * be 256 for a USGS geotiff file, of which only the first
   * 13 are used.  Other types of geotiff's may use more
   * colors (and do).  A proposed revision to the USGS DRG spec
   * allows using more colors.
   */
  num_colors = (1L << bitsPerSample);


  /* Print out the colormap info */
  //if (debug_level & 16) {
  //    int l;
  //
  //    for (l = 0; l < num_colors; l++)
  //    fprintf(stderr,"   %5u: %5u %5u %5u\n",
  //    l,
  //    red_orig[l],
  //    green_orig[l],
  //    blue_orig[l]);
  //}
  // Example output from a USGS 7.5' map:
  //
  //   0:     0     0     0   black
  //   1: 65280 65280 65280   light grey
  //   2:     0 38656 41984
  //   3: 51968     0  5888
  //   4: 33536 16896  9472   contour lines, brownish-red
  //   5: 51456 59904 40192   green
  //   6: 35072 13056 32768   purple?  freeways, some roads
  //   7: 65280 59904     0
  //   8: 42752 57856 57856   blue, bodies of water
  //   9: 65280 47104 47104   red brick color, cities?
  //  10: 55808 45824 54784   purple.  freeways, some roads
  //  11: 53504 53504 53504   grey
  //  12: 52992 41984 36352   contour lines, tan, more dots/less lines
  //  13:     0     0     0   black, unused slot?
  //  14:     0     0     0   black, unused slot?
  //  15:     0     0     0   black, unused slot?
  //  16:     0     0     0   black, unused slot?
  // The rest are all 0's.


  if (crop_it)    // USGS geoTIFF map
  {
    /*
     * Next:
     * Convert the map neat-line corners to image x/y coordinates.
     * This will give the map neat-line coordinates in pixels.
     * Use this data to chop the image at these boundaries
     * and to stretch the shorter lines to fit a rectangle.
     *
     * Note that at this stage we're using the bounding coordinates
     * that are in the map original datum so that the translations
     * to pixel coordinates will be correct.
     *
     * Note that we already have the datum-shifted values for all
     * the corners in the *_wgs84 variables.  In short:  We use the
     * non datum-shifted values to work with the tiff file, and the
     * datum-shifted values to plot the points in Xastir.
     */

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
    {
      GTIFFree (gtif);
      XTIFFClose (tif);
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

    if (debug_level & 16)
      fprintf(stderr,"\nNW neat-line corner = %f\t%f\n",
              f_NW_x_bounding,
              f_NW_y_bounding);

    xxx = (double)f_NW_x_bounding;
    yyy = (double)f_NW_y_bounding;

    /* Convert lat/long to projected coordinates */
    if (  proj_is_latlong || GTIFProj4FromLatLong( &defn, 1, &xxx, &yyy ) )       // Do all 4 in one call?
    {

      if (debug_level & 16)
      {
        fprintf(stderr,"%11.3f,%11.3f\n", xxx, yyy);
      }

      /* Convert from PCS coordinates to image pixel coordinates */
      if ( GTIFPCSToImage( gtif, &xxx, &yyy ) )   // Do all 4 in one call?
      {

        if (debug_level & 16)
        {
          fprintf(stderr,"X/Y Pixels: %f, %f\n", xxx, yyy);
        }

        NW_x = (int)(xxx + 0.5);    /* Tricky way of rounding */
        NW_y = (int)(yyy + 0.5);    /* Tricky way of rounding */

        if (debug_level & 16)
        {
          fprintf(stderr,"X/Y Pixels: %d, %d\n", NW_x, NW_y);
        }

        if (NW_x < 0 || NW_y < 0 || NW_x >= (int)width || NW_y >= (int)height)
        {

          fprintf(stderr,
                  "\nWarning:  NW Neat-line corner calculated at x:%d, y:%d, %s\n",
                  NW_x,
                  NW_y,
                  filenm);
          fprintf(stderr,
                  "Limits are: 0,0 and %ld,%ld. Resetting corner position.\n",
                  (long int)width,
                  (long int)height);
          fprintf(stderr,
                  "Map may appear in the wrong location or scale incorrectly.\n");

          if (NW_x < 0)
          {
            NW_x = 0;
          }

          if (NW_x >= (int)width)
          {
            NW_x = width - 1;
          }

          if (NW_y < 0)
          {
            NW_y = 0;
          }

          if (NW_y >= (int)height)
          {
            NW_y = height -1;
          }

          /*
          //Free any memory used and return
          // We're finished with the geoTIFF key parser, so get rid of it
          GTIFFree (gtif);

          // Close the TIFF file descriptor
          XTIFFClose (tif);

          return;
          */
        }
      }
    }
    else
    {
      fprintf(stderr,"Problem in translating\n");
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
    {
      GTIFFree (gtif);
      XTIFFClose (tif);
      // Update to screen
      (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
      return;
    }

    if (debug_level & 16)
      fprintf(stderr,"NE neat-line corner = %f\t%f\n",
              f_NE_x_bounding,
              f_NE_y_bounding);

    xxx = (double)f_NE_x_bounding;
    yyy = (double)f_NE_y_bounding;

    /* Convert lat/long to projected coordinates */
    if (  proj_is_latlong || GTIFProj4FromLatLong( &defn, 1, &xxx, &yyy ) )
    {
      if (debug_level & 16)
      {
        fprintf(stderr,"%11.3f,%11.3f\n", xxx, yyy);
      }

      /* Convert from PCS coordinates to image pixel coordinates */
      if ( GTIFPCSToImage( gtif, &xxx, &yyy ) )
      {
        if (debug_level & 16)
        {
          fprintf(stderr,"X/Y Pixels: %f, %f\n", xxx, yyy);
        }

        NE_x = (int)(xxx + 0.5);    /* Tricky way of rounding */
        NE_y = (int)(yyy + 0.5);    /* Tricky way of rounding */

        if (debug_level & 16)
        {
          fprintf(stderr,"X/Y Pixels: %d, %d\n", NE_x, NE_y);
        }

        if (NE_x < 0 || NE_y < 0 || NE_x >= (int)width || NE_y >= (int)height)
        {

          fprintf(stderr,
                  "\nWarning:  NE Neat-line corner calculated at x:%d, y:%d, %s\n",
                  NE_x,
                  NE_y,
                  filenm);
          fprintf(stderr,
                  "Limits are: 0,0 and %ld,%ld. Resetting corner position.\n",
                  (long int)width,
                  (long int)height);
          fprintf(stderr,
                  "Map may appear in the wrong location or scale incorrectly.\n");

          if (NE_x < 0)
          {
            NE_x = 0;
          }

          if (NE_x >= (int)width)
          {
            NE_x = width - 1;
          }

          if (NE_y < 0)
          {
            NE_y = 0;
          }

          if (NE_y >= (int)height)
          {
            NE_y = height -1;
          }

          /*
          //Free any memory used and return
          // We're finished with the geoTIFF key parser, so get rid of it
          GTIFFree (gtif);

          // Close the TIFF file descriptor
          XTIFFClose (tif);

          return;
          */
        }
      }
    }
    else
    {
      fprintf(stderr,"Problem in translating\n");
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
    {
      GTIFFree (gtif);
      XTIFFClose (tif);
      // Update to screen
      (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
      return;
    }

    if (debug_level & 16)
      fprintf(stderr,"SW neat-line corner = %f\t%f\n",
              f_SW_x_bounding,
              f_SW_y_bounding);

    xxx = (double)f_SW_x_bounding;
    yyy = (double)f_SW_y_bounding;

    /* Convert lat/long to projected coordinates */
    if (  proj_is_latlong || GTIFProj4FromLatLong( &defn, 1, &xxx, &yyy ) )
    {
      if (debug_level & 16)
      {
        fprintf(stderr,"%11.3f,%11.3f\n", xxx, yyy);
      }

      /* Convert from PCS coordinates to image pixel coordinates */
      if ( GTIFPCSToImage( gtif, &xxx, &yyy ) )
      {
        if (debug_level & 16)
        {
          fprintf(stderr,"X/Y Pixels: %f, %f\n", xxx, yyy);
        }

        SW_x = (int)(xxx + 0.5);    /* Tricky way of rounding */
        SW_y = (int)(yyy + 0.5);    /* Tricky way of rounding */

        if (debug_level & 16)
        {
          fprintf(stderr,"X/Y Pixels: %d, %d\n", SW_x, SW_y);
        }

        if (SW_x < 0 || SW_y < 0 || SW_x >= (int)width || SW_y >= (int)height)
        {

          fprintf(stderr,
                  "\nWarning:  SW Neat-line corner calculated at x:%d, y:%d, %s\n",
                  SW_x,
                  SW_y,
                  filenm);
          fprintf(stderr,
                  "Limits are: 0,0 and %ld,%ld. Resetting corner position.\n",
                  (long int)width,
                  (long int)height);
          fprintf(stderr,
                  "Map may appear in the wrong location or scale incorrectly.\n");

          if (SW_x < 0)
          {
            SW_x = 0;
          }

          if (SW_x >= (int)width)
          {
            SW_x = width - 1;
          }

          if (SW_y < 0)
          {
            SW_y = 0;
          }

          if (SW_y >= (int)height)
          {
            SW_y = height -1;
          }

          /*
          //Free any memory used and return
          // We're finished with the geoTIFF key parser, so get rid of it
          GTIFFree (gtif);

          // Close the TIFF file descriptor
          XTIFFClose (tif);

          return;
          */
        }
      }
    }
    else
    {
      fprintf(stderr,"Problem in translating\n");
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
    {
      GTIFFree (gtif);
      XTIFFClose (tif);
      // Update to screen
      (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
      return;
    }

    if (debug_level & 16)
      fprintf(stderr,"SE neat-line corner = %f\t%f\n",
              f_SE_x_bounding,
              f_SE_y_bounding);

    xxx = (double)f_SE_x_bounding;
    yyy = (double)f_SE_y_bounding;

    /* Convert lat/long to projected coordinates */
    if (  proj_is_latlong || GTIFProj4FromLatLong( &defn, 1, &xxx, &yyy ) )
    {
      if (debug_level & 16)
      {
        fprintf(stderr,"%11.3f,%11.3f\n", xxx, yyy);
      }

      /* Convert from PCS coordinates to image pixel coordinates */
      if ( GTIFPCSToImage( gtif, &xxx, &yyy ) )
      {
        if (debug_level & 16)
        {
          fprintf(stderr,"X/Y Pixels: %f, %f\n", xxx, yyy);
        }

        SE_x = (int)(xxx + 0.5);    /* Tricky way of rounding */
        SE_y = (int)(yyy + 0.5);    /* Tricky way of rounding */

        if (debug_level & 16)
        {
          fprintf(stderr,"X/Y Pixels: %d, %d\n", SE_x, SE_y);
        }

        if (SE_x < 0 || SE_y < 0 || SE_x >= (int)width || SE_y >= (int)height)
        {

          fprintf(stderr,
                  "\nWarning:  SE Neat-line corner calculated at x:%d, y:%d, %s\n",
                  SE_x,
                  SE_y,
                  filenm);
          fprintf(stderr,
                  "Limits are: 0,0 and %ld,%ld. Resetting corner position.\n",
                  (long int)width,
                  (long int)height);
          fprintf(stderr,
                  "Map may appear in the wrong location or scale incorrectly.\n");

          if (SE_x < 0)
          {
            SE_x = 0;
          }

          if (SE_x >= (int)width)
          {
            SE_x = width - 1;
          }

          if (SE_y < 0)
          {
            SE_y = 0;
          }

          if (SE_y >= (int)height)
          {
            SE_y = height -1;
          }

          /*
          //Free any memory used and return
          // We're finished with the geoTIFF key parser, so get rid of it
          GTIFFree (gtif);

          // Close the TIFF file descriptor
          XTIFFClose (tif);

          return;
          */
        }
      }
    }
    else
    {
      fprintf(stderr,"Problem in translating\n");
    }
  }
  else    /*
             * No map collar to crop off, so we already know
             * where the corner points are.  This is for non-USGS
             * maps.
             */
  {
    NW_x = 0;
    NW_y = 0;

    NE_x = width - 1;
    NE_y = 0;

    SW_x = 0;
    SW_y = height - 1;

    SE_x = width - 1;
    SE_y = height - 1;
  }

  HandlePendingEvents(app_context);
  if (interrupt_drawing_now)
  {
    GTIFFree (gtif);
    XTIFFClose (tif);
    // Update to screen
    (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
    return;
  }

  // Here's where we crop off part of the black border for USGS maps.
  if (crop_it)    // USGS maps only
  {
    int i = 3;

    NW_x += i;
    NW_y += i;
    NE_x -= i;
    NE_y += i;
    SW_x += i;
    SW_y -= i;
    SE_x -= i;
    SE_y -= i;
  }


  // Now figure out the rough pixel crop values from what we know.
  // Image rotation means a simple rectangular crop isn't sufficient.
  if (NW_y < NE_y)
  {
    top_crop = NW_y;
  }
  else
  {
    top_crop = NE_y;
  }


  if (SW_y > SE_y)
  {
    bottom_crop = SW_y;
  }
  else
  {
    bottom_crop = SE_y;
  }


  if (NE_x > SE_x)
  {
    right_crop = NE_x;
  }
  else
  {
    right_crop = SE_x;
  }


  if (NW_x < SW_x)
  {
    left_crop = NW_x;
  }
  else
  {
    left_crop = SW_x;
  }


  if (!crop_it)       /* If we shouldn't crop the map collar... */
  {
    top_crop = 0;
    bottom_crop = height - 1;
    left_crop = 0;
    right_crop = width - 1;
  }

  // The four crop variables are the maximum rectangle that we
  // wish to keep, rotation notwithstanding (we may want to crop
  // part of some lines due to rotation).  Crop all lines/pixels
  // outside these ranges.

  //WE7U
  if (top_crop < 0 || top_crop >= (int)height)
  {
    top_crop = 0;
  }

  if (bottom_crop < 0 || bottom_crop >= (int)height)
  {
    bottom_crop = height - 1;
  }

  if (left_crop < 0 || left_crop >= (int)width)
  {
    left_crop = 0;
  }

  if (right_crop < 0 || right_crop >= (int)width)
  {
    right_crop = width - 1;
  }

  if (debug_level & 16)
  {
    fprintf(stderr,"Crop points (pixels):\n");
    fprintf(stderr,"Top: %d\tBottom: %d\tLeft: %d\tRight: %d\n",
            top_crop,
            bottom_crop,
            left_crop,
            right_crop);
  }


  /*
   * The color map is embedded in the geoTIFF file as TIFF tags.
   * We get those tags out of the file and translate to our own
   * colormap.
   * Allocate colors for the map image.  We allow up to 256 colors
   * and allow only 8-bits per pixel in the original map file.  We
   * get our 24-bit RGB colors right out of the map file itself, so
   * the colors should look right.
   * We're picking existing colormap colors that are closest to
   * the original map colors, so we shouldn't run out of colors
   * for other applications.
   *
   * Brightness adjust for the colors?  Implemented in the
   * "raster_map_intensity" variable below.
   */

  {
    int l;

    switch (photometric)
    {
      case PHOTOMETRIC_PALETTE:
        for (l = 0; l < num_colors; l++)
        {
          my_colors[l].red   =   (uint16)(red_orig[l] * raster_map_intensity);
          my_colors[l].green = (uint16)(green_orig[l] * raster_map_intensity);
          my_colors[l].blue  =  (uint16)(blue_orig[l] * raster_map_intensity);

          if (visual_type == NOT_TRUE_NOR_DIRECT)
          {
            //                    XFreeColors(XtDisplay(w), cmap, &(my_colors[l].pixel),1,0);
            XAllocColor(XtDisplay(w), cmap, &my_colors[l]);
          }
          else
          {
            pack_pixel_bits(my_colors[l].red, my_colors[l].green, my_colors[l].blue,
                            &my_colors[l].pixel);
          }
        }
        break;
      case PHOTOMETRIC_MINISBLACK:
        for (l = 0; l < num_colors; l++)
        {
          int v = (l * 255) / (num_colors-1);
          my_colors[l].red = my_colors[l].green = my_colors[l].blue =
              (uint16)(v * raster_map_intensity) << 8;

          if (visual_type == NOT_TRUE_NOR_DIRECT)
          {
            //                    XFreeColors(XtDisplay(w), cmap, &(my_colors[l].pixel),1,0);
            XAllocColor(XtDisplay(w), cmap, &my_colors[l]);
          }
          else
          {
            pack_pixel_bits(my_colors[l].red, my_colors[l].green, my_colors[l].blue,
                            &my_colors[l].pixel);
          }
        }
        break;
      case PHOTOMETRIC_MINISWHITE:
        for (l = 0; l < num_colors; l++)
        {
          int v = (((num_colors-1)-l) * 255) / (num_colors-1);
          my_colors[l].red = my_colors[l].green = my_colors[l].blue =
              (uint16)(v * raster_map_intensity) << 8;

          if (visual_type == NOT_TRUE_NOR_DIRECT)
          {
            //                    XFreeColors(XtDisplay(w), cmap, &(my_colors[l].pixel),1,0);
            XAllocColor(XtDisplay(w), cmap, &my_colors[l]);
          }
          else
          {
            pack_pixel_bits(my_colors[l].red, my_colors[l].green, my_colors[l].blue,
                            &my_colors[l].pixel);
          }
        }
        break;
    }
  }

  HandlePendingEvents(app_context);
  if (interrupt_drawing_now)
  {
    GTIFFree (gtif);
    XTIFFClose (tif);
    // Update to screen
    (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
    return;
  }

  // Each data value should be an 8-bit value, which is a
  // pointer into a color
  // table.  Later we perform a translation from the geoTIFF
  // color table to our current color table (matching values
  // as close as possible), at the point where we're writing
  // the image to the pixmap.


  /* We should be ready now to actually read in some
   * pixels and deposit them on the screen.  We will
   * allocate memory for the data area based on the
   * sizes of fields and data in the geoTIFF file.
   */

  bytesPerRow = TIFFScanlineSize(tif);

  if (debug_level & 16)
  {
    fprintf(stderr,"\nInitial Bytes Per Row: %d\n", bytesPerRow);
  }


  // Here's a tiny malloc that'll hold only one scanline worth of pixels
  imageMemory = (u_char *) malloc(bytesPerRow + 2);
  CHECKMALLOC(imageMemory);


  // TODO:  Figure out the middle boundary on each edge for
  // lat/long and adjust the crop values to match the largest
  // of either the middle or the corners for each edge.  This
  // will help to handle edges that are curved.


  /*
   * There are some optimizations that can still be done:
   *
   * 1) Read in all scanlines but throw away unneeded pixels,
   *    paying attention not to lose smaller details.  Compare
   *    neighboring pixels?
   *
   * 3) Keep a map cache or a screenmap cache to reduce need
   *    for reading map files so often.
   */



  // Here we wish to start at the top line that may have
  // some pixels of interest and proceed to the bottom line
  // of interest.  Process scanlines from top_crop to bottom_crop.
  // Start at the left/right_crop pixels, compute the lat/long
  // of each, using x/y increments so we can quickly scan across
  // the line.
  // Iterate across the line checking whether each pixel is
  // within the viewport.  If so, plot it on the pixmap at
  // the correct scale.

  // Later I may wish to get the lat/lon of each pixel and plot
  // it at the correct point, to handle the curvature of each
  // line (this might be VERY slow).  Right now I treat them as
  // straight lines.

  // At this point we have these variables defined.  The
  // first column contains map corners in Xastir coordinates,
  // the second column contains map corners in pixels:
  //
  // NW corner:
  // NW_x_bounding_wgs84  <-> NW_x
  // NW_y_bounding_wgs84  <-> NW_y
  //
  // NE corner:
  // NE_x_bounding_wgs84  <-> NE_x
  // NE_y_bounding_wgs84  <-> NE_y
  //
  // SW corner:
  // SW_x_bounding_wgs84  <-> SW_x
  // SW_y_bounding_wgs84  <-> SW_y
  //
  // SE corner:
  // SE_x_bounding_wgs84  <-> SE_x
  // SE_y_bounding_wgs84  <-> SE_y

  // I should be able to use these variables to figure out
  // the xastir coordinates of each scanline pixel using
  // linear interpolation along each edge.
  //
  // I don't want to use the crop values in general.  I'd
  // rather crop properly along the neat line instead of a
  // rectangular crop.
  //
  // Define lines along the left/right edges so that I can
  // compute the Xastir coordinates of each pixel along these
  // two lines.  These will be the start/finish of each of my
  // scanlines, and I can use these values to compute the
  // x/y_increment values for each line.  This way I can
  // stretch short lines as I go along, and auto-crop the
  // white border as well.

  // Left_line goes from (top to bottom):
  // NW_x,NW_y -> SW_x,SW_y
  // and from:
  // west_bounding_wgs84,north_bounding_wgs84 -> west_bounding_wgs84,south_bounding_wgs84
  //
  // Right_line goes from(top to bottom):
  // NE_x,NE_y -> SE_x,SE-Y
  // and from:
  // east_bounding_wgs84,north_bounding_wgs84 -> east_bounding_wgs84,south_bounding_wgs84
  //
  // Simpler:  Along each line, Xastir coordinates change how much
  // and in what direction as we move down one scanline?


  // These increments are how much we change in Xastir coordinates and
  // in pixel coordinates as we move down either the left or right
  // neatline one pixel.
  // Be prepared for 0 angle of rotation as well (x-increments = 0).


  // Xastir Coordinate System:
  //
  //              0 (90 deg. or 90N)
  //
  // 0 (-180 deg. or 180W)      129,600,000 (180 deg. or 180E)
  //
  //          64,800,000 (-90 deg. or 90S)


  // Watch out for division by zero here.


  //
  // Left Edge X Increment Per Scanline (Going from top to bottom).
  // This increment will help me to keep track of the left edge of
  // the image, both in Xastir coordinates and in pixel coordinates.
  //
  if (SW_y != NW_y)
  {
    // Xastir coordinates
    xastir_left_x_increment = (float)
                              (1.0 * labs( (long)SW_x_bounding_wgs84 - (long)NW_x_bounding_wgs84 )   // Need to add one pixel worth here yet
                               / abs(SW_y - NW_y));

    // Pixel coordinates
    left_x_increment = (float)(1.0 * abs(SW_x - NW_x)
                               / abs(SW_y - NW_y));

    if (SW_x_bounding_wgs84 < NW_x_bounding_wgs84)
    {
      xastir_left_x_increment = -xastir_left_x_increment;
    }

    if (SW_x < NW_x)
    {
      left_x_increment = -left_x_increment;
    }

    //WE7U
    //if (abs(left_x_increment) > (width/10)) {
    //    left_x_increment = 0.0;
    //    xastir_left_x_increment = 0.0;
    //}

    if (debug_level & 16)
      fprintf(stderr,"xastir_left_x_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
              xastir_left_x_increment,
              SW_x_bounding_wgs84,
              NW_x_bounding_wgs84,
              left_x_increment,
              SW_x,
              NW_x,
              bottom_crop,
              top_crop);
  }
  else
  {
    // Xastir coordinates
    xastir_left_x_increment = 0;

    // Pixel coordinates
    left_x_increment = 0;
  }


  //
  // Left Edge Y Increment Per Scanline (Going from top to bottom)
  // This increment will help me to keep track of the left edge of
  // the image, both in Xastir coordinates and in pixel coordinates.
  //
  if (SW_y != NW_y)
  {
    // Xastir coordinates
    xastir_left_y_increment = (float)
                              (1.0 * labs( (long)SW_y_bounding_wgs84 - (long)NW_y_bounding_wgs84 )   // Need to add one pixel worth here yet
                               / abs(SW_y - NW_y));

    // Pixel coordinates
    left_y_increment = (float)1.0; // Aren't we going down one pixel each time?

    if (SW_y_bounding_wgs84 < NW_y_bounding_wgs84)  // Ain't gonn'a happen
    {
      xastir_left_y_increment = -xastir_left_y_increment;
    }

    //WE7U
    //if (abs(left_y_increment) > (width/10)) {
    //    xastir_left_y_increment = 0.0;
    //}

    if (debug_level & 16)
      fprintf(stderr,"xastir_left_y_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
              xastir_left_y_increment,
              SW_y_bounding_wgs84,
              NW_y_bounding_wgs84,
              left_y_increment,
              SW_y,
              NW_y,
              bottom_crop,
              top_crop);
  }
  else
  {
    // Xastir coordinates
    xastir_left_y_increment = 0;

    // Pixel coordinates
    left_y_increment = 0;
  }


  //
  // Right Edge X Increment Per Scanline (Going from top to bottom)
  // This increment will help me to keep track of the right edge of
  // the image, both in Xastir coordinates and image coordinates.
  //
  if (SE_y != NE_y)
  {
    // Xastir coordinates
    xastir_right_x_increment = (float)
                               (1.0 * labs( (long)SE_x_bounding_wgs84 - (long)NE_x_bounding_wgs84 )   // Need to add one pixel worth here yet
                                / abs(SE_y - NE_y));

    // Pixel coordinates
    right_x_increment = (float)(1.0 * abs(SE_x - NE_x)
                                / abs(SE_y - NE_y));

    if (SE_x_bounding_wgs84 < NE_x_bounding_wgs84)
    {
      xastir_right_x_increment = -xastir_right_x_increment;
    }

    if (SE_x < NE_x)
    {
      right_x_increment = -right_x_increment;
    }

    //WE7U
    //if (abs(right_x_increment) > (width/10)) {
    //    right_x_increment = 0.0;
    //    xastir_right_x_increment = 0.0;
    //}

    if (debug_level & 16)
      fprintf(stderr,"xastir_right_x_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
              xastir_right_x_increment,
              SE_x_bounding_wgs84,
              NE_x_bounding_wgs84,
              right_x_increment,
              SE_x,
              NE_x,
              bottom_crop,
              top_crop);
  }
  else
  {
    // Xastir coordinates
    xastir_right_x_increment = 0;

    // Pixel coordinates
    right_x_increment = 0;
  }


  //
  // Right Edge Y Increment Per Scanline (Going from top to bottom)
  // This increment will help me to keep track of the right edge of
  // the image, both in Xastir coordinates and in image coordinates.
  //
  if (SE_y != NE_y)
  {
    // Xastir coordinates
    xastir_right_y_increment = (float)
                               (1.0 * labs( (long)SE_y_bounding_wgs84 - (long)NE_y_bounding_wgs84 )   // Need to add one pixel worth here yet
                                / abs(SE_y - NE_y));

    // Pixel coordinates
    right_y_increment = (float)1.0;    // Aren't we going down one pixel each time?

    if (SE_y_bounding_wgs84 < NE_y_bounding_wgs84)  // Ain't gonn'a happen
    {
      xastir_right_y_increment = -xastir_right_y_increment;
    }

    //WE7U
    //if (abs(right_y_increment) > (width/10)) {
    //    xastir_right_y_increment = 0.0;
    //}

    if (debug_level & 16)
      fprintf(stderr,"xastir_right_y_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
              xastir_right_y_increment,
              SE_y_bounding_wgs84,
              NE_y_bounding_wgs84,
              right_y_increment,
              SE_y,
              NE_y,
              bottom_crop,
              top_crop);
  }
  else
  {
    // Xastir coordinates
    xastir_right_y_increment = 0;

    // Pixel coordinates
    right_y_increment = 0;
  }


  if (debug_level & 16)
  {
    fprintf(stderr," Left x increments: %f %f\n", xastir_left_x_increment, left_x_increment);
    fprintf(stderr," Left y increments: %f %f\n", xastir_left_y_increment, left_y_increment);
    fprintf(stderr,"Right x increments: %f %f\n", xastir_right_x_increment, right_x_increment);
    fprintf(stderr,"Right y increments: %f %f\n", xastir_right_y_increment, right_y_increment);
  }


  // Compute how much "y" changes per pixel as we traverse from left to right
  // along a scanline along the top of the image.
  //
  // Top Edge Y Increment Per X-Pixel Width (Going from left to right).
  // This increment will help me to get rid of image rotation.
  //
  if (NE_x != NW_x)
  {
    // Xastir coordinates
    xastir_top_y_increment = (float)
                             (1.0 * labs( (long)NE_y_bounding_wgs84 - (long)NW_y_bounding_wgs84 )   // Need to add one pixel worth here yet
                              / abs(NE_x - NW_x));    // And a "+ 1.0" here?

    // Pixel coordinates
    top_y_increment = (float)(1.0 * abs(NE_y - NW_y)
                              / abs(NE_x - NW_x));

    if (NE_y_bounding_wgs84 < NW_y_bounding_wgs84)
    {
      xastir_top_y_increment = -xastir_top_y_increment;
    }

    if (NE_y < NW_y)
    {
      top_y_increment = -top_y_increment;
    }

    if (debug_level & 16)
      fprintf(stderr,"xastir_top_y_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
              xastir_top_y_increment,
              NE_y_bounding_wgs84,
              NW_y_bounding_wgs84,
              top_y_increment,
              NE_y,
              NW_y,
              right_crop,
              left_crop);
  }
  else
  {
    // Xastir coordinates
    xastir_top_y_increment = 0;

    // Pixel coordinates
    top_y_increment = 0;
  }


  // Compute how much "y" changes per pixel as you traverse from left to right
  // along a scanline along the bottom of the image.
  //
  // Bottom Edge Y Increment Per X-Pixel Width (Going from left to right).
  // This increment will help me to get rid of image rotation.
  //
  if (SE_x != SW_x)
  {
    // Xastir coordinates
    xastir_bottom_y_increment = (float)
                                (1.0 * labs( (long)SE_y_bounding_wgs84 - (long)SW_y_bounding_wgs84 )   // Need to add one pixel worth here yet
                                 / abs(SE_x - SW_x));    // And a "+ 1.0" here?

    // Pixel coordinates
    bottom_y_increment = (float)(1.0 * abs(SE_y - SW_y)
                                 / abs(SE_x - SW_x));

    if (SE_y_bounding_wgs84 < SW_y_bounding_wgs84)
    {
      xastir_bottom_y_increment = -xastir_bottom_y_increment;
    }

    if (SE_y < SW_y)
    {
      bottom_y_increment = -bottom_y_increment;
    }

    if (debug_level & 16)
      fprintf(stderr,"xastir_bottom_y_increment: %f  %ld  %ld     %f  %d  %d  %d  %d\n",
              xastir_bottom_y_increment,
              SE_y_bounding_wgs84,
              SW_y_bounding_wgs84,
              bottom_y_increment,
              SE_y,
              SW_y,
              right_crop,
              left_crop);
  }
  else
  {
    // Xastir coordinates
    xastir_bottom_y_increment = 0;

    // Pixel coordinates
    bottom_y_increment = 0;
  }


  // Find the average change in Y as we traverse from left to right one pixel
  //    xastir_avg_y_increment = (float)(xastir_top_y_increment + xastir_bottom_y_increment) / 2.0;
  avg_y_increment = (float)(top_y_increment + bottom_y_increment) / 2.0;


  // Find edges of current viewport in Xastir coordinates
  //
  view_min_x = NW_corner_longitude;  // left edge of view
  if (view_min_x > 129600000l)
  {
    view_min_x = 0;
  }

  view_max_x = SE_corner_longitude; // right edge of view
  if (view_max_x > 129600000l)
  {
    view_max_x = 129600000l;
  }

  view_min_y = NW_corner_latitude;   // top edge of view
  if (view_min_y > 64800000l)
  {
    view_min_y = 0;
  }

  view_max_y = SE_corner_latitude;  // bottom edge of view
  if (view_max_y > 64800000l)
  {
    view_max_y = 64800000l;
  }


  /* Get the pixel scale */
  have_PixelScale = TIFFGetField( tif, TIFFTAG_GEOPIXELSCALE, &qty, &PixelScale );
  if (debug_level & 16)
  {
    if (have_PixelScale)
    {
      fprintf(stderr,"PixelScale: %f %f %f\n",
              *PixelScale,
              *(PixelScale + 1),
              *(PixelScale + 2) );
    }
    else
    {
      fprintf(stderr,"No PixelScale tag found in file\n");
    }
  }


  // Use PixelScale to determine lines to skip at each
  // zoom level?
  // O-size map:
  // ModelPixelScaleTag (1,3):
  //   2.4384           2.4384           0
  //
  // F-size map:
  // ModelPixelScaleTag (1,3):
  //   10.16            10.16            0
  //
  // C-size map:
  // ModelPixelScaleTag (1,3):
  //   25.400001        25.400001        0
  //

  if (debug_level & 16)
  {
    fprintf(stderr,"Size: x %ld, y %ld\n", scale_x,scale_y);
  }


  // I tried to be very aggressive with the scaling factor
  // below (3.15) in order to skip the most possible rows
  // to speed things up.  If you see diagonal lines across
  // the maps, increase this number (up to a max of 4.0
  // probably).  A higher number means less rows skipped,
  // which improves the look but slows the map drawing down.
  //
  // This needs modification somewhat when the raster is already in
  // lat/long, and the PixelScale is already in degrees per pixel.
  // The scale_y is the integer number of hundredths of seconds per pixel.
  // The decision of how many rows to skip can simply be made by converting
  // the PixelScale to the same units as scale_y, and skipping accordingly.

  if (have_PixelScale)
  {
    double coef;
    if (!proj_is_latlong)
    {
      coef = 3.15;
    }
    else
    {
      coef=100*60*60;  // xastir coords are in 1/100 of a second,
      // and lat/lon pixel scales will be in degrees
    }
    SkipRows = (int)( ( scale_y / ( *PixelScale * coef ) ) + 0.5 );
    if (SkipRows < 1)
    {
      SkipRows = 1;
    }
    if (SkipRows > (int)(height / 10) )
    {
      SkipRows = height / 10;
    }
  }
  else
  {
    SkipRows = 1;
  }
  if (debug_level & 16)
  {
    fprintf(stderr,"SkipRows: %d\n", SkipRows);
  }

  // Use SkipRows to set increments for the loops below.


  if ( top_crop <= 0 )
  {
    top_crop = 0;  // First row of image
  }

  if ( ( bottom_crop + 1) >= (int)height )
  {
    bottom_crop = height - 1;  // Last row of image
  }

  // Here I pre-compute some of the values I'll need in the
  // loops below in order to save some time.
  NW_line_offset = (int)(NW_y - top_crop);
  NE_line_offset = (int)(NE_y - top_crop);

  NW_xastir_x_offset =  (int)(xastir_left_x_increment * NW_line_offset);
  NE_xastir_x_offset = (int)(xastir_right_x_increment * NE_line_offset);
  NW_xastir_y_offset =  (int)(xastir_left_y_increment * NW_line_offset);

  NW_x_offset =  (int)(1.0 * left_x_increment * NW_line_offset);
  NE_x_offset = (int)(1.0 * right_x_increment * NE_line_offset);
  xastir_avg_left_right_y_increment = (float)((xastir_right_y_increment + xastir_left_y_increment) / 2.0);
  total_avg_y_increment = (float)(xastir_avg_left_right_y_increment * avg_y_increment);


  // (Xastir bottom - Xastir top) / height
  //steph = (double)( (left_y_increment + right_y_increment) / 2);
  // NOTE:  This one does not take into account current height
  steph = (float)( (SW_y_bounding_wgs84 - NW_y_bounding_wgs84)
                   / (1.0 * (SW_y - NW_y) ) );

  // Compute scaled pixel size for XFillRectangle
  stephc = (int)( ( (1.50 * steph / scale_x) + 1.0) + 1.5);

  view_top_minus_pixel_height = (unsigned long)(view_min_y - steph);


  HandlePendingEvents(app_context);
  if (interrupt_drawing_now)
  {
    if (imageMemory)
    {
      free(imageMemory);
    }
    GTIFFree (gtif);
    XTIFFClose (tif);
    // Update to screen
    (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
    return;
  }


  // Set the bit-blitting function to XOR.  This is only used if
  // DRG_XOR_colors is set to a non-zero value, but we set it here
  // outside the pixel loop for speed reasons.
  //
  if (DRG_XOR_colors)
  {
    (void)XSetLineAttributes (XtDisplay (w), gc_tint, 1, LineSolid, CapButt,JoinMiter);

    //        (void)XSetForeground (XtDisplay (w), gc_tint, colors[0x27]);  // yellow
    //        (void)XSetForeground (XtDisplay (w), gc_tint, colors[0x0f]); // White
    //        (void)XSetForeground (XtDisplay (w), gc_tint, colors[0x03]); // cyan
    //        (void)XSetForeground (XtDisplay (w), gc_tint, colors[0x06]); // orange

    (void)XSetFunction (XtDisplay (da), gc_tint, GXxor);
  }


  // Iterate over the rows of interest only.  Using the rectangular
  // top/bottom crop values for these is ok at this point.
  //
  // Put row multipliers above loops.  Try to get as many
  // multiplications as possible outside the loops.  Adds and
  // subtracts are ok.  Try to do as little floating point stuff
  // as possible inside the loops.
  //
  for ( row = top_crop; (int)row < bottom_crop + 1; row+= SkipRows )
  {
    int skip = 0;


    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
    {
      if (imageMemory)
      {
        free(imageMemory);
      }
      GTIFFree (gtif);
      XTIFFClose (tif);
      // Update to screen
      (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
      return;
    }

    // Our offset from the top row of the map neatline
    // (kind of... ignoring rotation anyway).
    row_offset = row - top_crop;
    //fprintf(stderr,"row_offset: %d\n", row_offset);


    // Compute the line end-points in Xastir coordinates
    // Initially was a problem here:  Offsetting from NW_x_bounding but
    // starting at top_crop line.  Fixed by last term added to two
    // equations below.

    current_xastir_left = (unsigned long)
                          ( NW_x_bounding_wgs84
                            + ( 1.0 * xastir_left_x_increment * row_offset )
                            -   NW_xastir_x_offset );

    current_xastir_right = (unsigned long)
                           ( NE_x_bounding_wgs84
                             + ( 1.0 * xastir_right_x_increment * row_offset )
                             -   NE_xastir_x_offset );


    //if (debug_level & 16)
    //  fprintf(stderr,"Left: %ld  Right:  %ld\n",
    //      current_xastir_left,
    //      current_xastir_right);


    // In pixel coordinates:
    current_left = (int)
                   ( NW_x
                     + ( 1.0 * left_x_increment * row_offset ) + 0.5
                     -   NW_x_offset );

    current_right = (int)
                    ( NE_x
                      + ( 1.0 * right_x_increment * row_offset ) + 0.5
                      -   NE_x_offset );

    //WE7U:  This comparison is always false:  current_left is unsigned
    //therefore always positive!
    //if (current_left < 0)
    //    current_left = 0;

    if (current_right >= width)
    {
      current_right = width - 1;
    }

    //        current_line_width = current_right - current_left + 1;  // Pixels


    // if (debug_level & 16)
    //     fprintf(stderr,"Left: %ld  Right: %ld  Width: %ld\n",
    //         current_left,
    //         current_right, current_line_width);


    // Compute original pixel size in Xastir coordinates.  Note
    // that this can change for each scanline in a USGS geoTIFF.

    // (Xastir right - Xastir left) / width-of-line
    // Need the "1.0 *" or the math will be incorrect (won't be a float)
    stepw = (float)( (current_xastir_right - current_xastir_left)
                     / (1.0 * (current_right - current_left) ) );



    // if (debug_level & 16)
    //     fprintf(stderr,"\t\t\t\t\t\tPixel Width: %f\n",stepw);

    // Compute scaled pixel size for XFillRectangle
    stepwc = (int)( ( (1.0 * stepw / scale_x) + 1.0) + 0.5);


    // In Xastir coordinates
    xastir_current_y = (unsigned long)(NW_y_bounding_wgs84
                                       + (xastir_left_y_increment * row_offset) );

    xastir_current_y = (unsigned long)(xastir_current_y - NW_xastir_y_offset);


    view_left_minus_pixel_width = view_min_x - stepw;


    // Check whether any part of the scanline will be within the
    // view.  If so, read the scanline from the file and iterate
    // across the pixels.  If not, skip this line altogether.

    // Compute right edge of image
    xastir_total_y = (unsigned long)
                     ( xastir_current_y
                       - ( total_avg_y_increment * (current_right - current_left) ) );

    // Check left edge y-value then right edge y-value.
    // If either are within view, process the line, else skip it.
    if ( ( ( xastir_current_y <= view_max_y) && (xastir_total_y >= view_top_minus_pixel_height) )
         || ( ( xastir_total_y <= view_max_y ) && ( xastir_total_y >= view_top_minus_pixel_height ) ) )
    {
      // Read one geoTIFF scanline
      if (TIFFReadScanline(tif, imageMemory, row, 0) < 0)
      {
        break;  // No more lines to read or we couldn't read the file at all
      }


      // Iterate over the columns of interest, skipping the left/right
      // cropped pixels, looking for pixels that fit within our viewport.
      //
      for ( column = current_left; column < (current_right + 1); column++ )
      {
        skip = 0;


        column_offset = column - current_left;  // Pixels

        //fprintf(stderr,"Column Offset: %ld\n", column_offset);  // Pixels
        //fprintf(stderr,"Current Left: %ld\n", current_left);    // Pixels

        xastir_current_x = (unsigned long)
                           current_xastir_left
                           + (stepw * column_offset);    // In Xastir coordinates

        // Left line y value minus
        // avg y-increment per scanline * avg y-increment per x-pixel * column_offset
        xastir_total_y = (unsigned long)
                         ( xastir_current_y
                           - ( total_avg_y_increment * column_offset ) );

        //fprintf(stderr,"Xastir current: %ld %ld\n", xastir_current_x, xastir_current_y);


        // Check whether pixel fits within boundary lines (USGS maps)
        // This is how we get rid of the last bit of white border at
        // the top and bottom of the image.
        if (have_fgd)   // USGS map
        {
          if (   (xastir_total_y > SW_y_bounding_wgs84)
                 || (xastir_total_y < NW_y_bounding_wgs84) )
          {
            skip++;
          }


          // Here's a trick to make it look like the map pages join better.
          // If we're within a certain distance of a border, change any black
          // pixels to white (changes map border to less obtrusive color).
          if ( *(imageMemory + column) == 0x00 )  // If pixel is Black
          {
            if ( (xastir_total_y > (SW_y_bounding_wgs84 - 25) )
                 || (xastir_total_y < (NW_y_bounding_wgs84 + 25) )
                 || (xastir_current_x < (SW_x_bounding_wgs84 + 25) )
                 || (xastir_current_x > (SE_x_bounding_wgs84 - 25) ) )
            {
              //WE7U: column is unsigned so "column >= 0" is always true
              //                            if ((int)column < bytesPerRow && column >= 0) {
              if ((int)column < bytesPerRow)
              {

                *(imageMemory + column) = 0x01; // Change to White
              }
              else
              {
                //WE7U
                //                                fprintf(stderr,"draw_geotiff_image_map: Bad fgd file for map?: %s\n", filenm);
              }
            }
          }
        }


        /* Look for left or right map boundaries inside view */
        if ( !skip
             && ( xastir_current_x <= view_max_x )
             && ( xastir_current_x >= view_left_minus_pixel_width )
             && ( xastir_total_y <= view_max_y )
             && ( xastir_total_y >= view_top_minus_pixel_height ) )
        {
          // Here are the corners of our viewport, using the Xastir
          // coordinate system.  Notice that Y is upside down:
          //
          // left edge of view = NW_corner_longitude
          // right edge of view = SE_corner_longitude
          // top edge of view =  NW_corner_latitude
          // bottom edge of view =  SE_corner_latitude


          // Compute the screen position of the pixel and scale it
          sxx = (xastir_current_x - NW_corner_longitude) / scale_x;
          syy = (xastir_total_y   - NW_corner_latitude ) / scale_y;



          // If we wish some colors to be transparent, we could check for a
          // color match here and refuse to draw those that are on our list of
          // transparent colors.  Might be useful for drawing say contour
          // lines on top of satellite images.  Since the USGS topo's use the
          // same colormap for all of the topo's (I believe that is true) then
          // we could just compare on the index into the array instead of the
          // color contents in the array.
          //
          // Example output from a USGS 7.5' map:
          //
          //   0:     0     0     0   black
          //   1: 65280 65280 65280   light grey
          //   2:     0 38656 41984
          //   3: 51968     0  5888
          //   4: 33536 16896  9472   contour lines, brownish-red
          //   5: 51456 59904 40192   green
          //   6: 35072 13056 32768   purple?  freeways, some roads
          //   7: 65280 59904     0
          //   8: 42752 57856 57856   blue, bodies of water
          //   9: 65280 47104 47104   red brick color, cities?
          //  10: 55808 45824 54784   purple.  freeways, some roads
          //  11: 53504 53504 53504   grey
          //  12: 52992 41984 36352   contour lines, tan, more dots/less lines
          //  13:     0     0     0   black, unused slot?
          //  14:     0     0     0   black, unused slot?
          //  15:     0     0     0   black, unused slot?
          //  16:     0     0     0   black, unused slot?
          // The rest are all 0's.
          // either show all colors (usgs_drg==0) or show selected
          // colors (usgs_drg == 1)
          if ( usgs_drg == 0 ||
               (usgs_drg ==1 && (*(imageMemory + column) >= 13
                                 || DRG_show_colors[*(imageMemory + column)] == 1)))
          {


            // If this is set, use gc_tint for drawing
            // with "GXxor" as the bit-blit operation.
            //
            if (DRG_XOR_colors)
            {

              // Draw the pixel using "gc_tint"
              // instead of "gc".

              XSetForeground (XtDisplay (w), gc_tint, my_colors[*(imageMemory + column)].pixel);

              XFillRectangle (XtDisplay (w), pixmap, gc_tint, sxx, syy, stepwc, stephc);
            }
            else
            {

              // Set the color for the pixel
              XSetForeground (XtDisplay (w), gc, my_colors[*(imageMemory + column)].pixel);

              // And draw the pixel
              XFillRectangle (XtDisplay (w), pixmap, gc, sxx, syy, stepwc, stephc);
            }
          }
        }
      }
    }
  }


  /* Free up any malloc's that we did */
  if (imageMemory)
  {
    free(imageMemory);
  }


  if (debug_level & 16)
  {
    fprintf(stderr,"%d rows read in\n", (int) row);
  }

  /* We're finished with the geoTIFF key parser, so get rid of it */
  GTIFFree (gtif);

  /* Close the TIFF file descriptor */
  XTIFFClose (tif);

  // Close the filehandles that are left open after the
  // four GTIFImageToPCS calls.
  //(void)CSVDeaccess(NULL);
}
#endif /* HAVE_LIBGEOTIFF */


