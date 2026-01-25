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
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include <stdlib.h>

#include "snprintf.h"
#include "xastir.h"
#include "main.h"
#include "datum.h"

// To produce MGRS coordinates, we'll call ll_to_utm_ups()
// [?? Earlier text: "the above function" ??]
// then convert the result to the 2-letter digraph format used
// for MGRS.  The ll_to_utm_ups() function switches to the special
// irregular UTM zones for the areas near Svalbard and SW Norway
// if the "coordinate_system" variable is set to "USE_MGRS",
// so we'll be using the correct zone boundaries for MGRS if that
// variable is set when we make the call.
//
// If "nice_format" == 1, we add leading spaces plus spaces between
// the easting and northing in order to line up more nicely with the
// UTM output format.
//
// convert_xastir_to_MGRS_str is a wrapper around the components
// function below that returns the MGRS coordinate of x and y as a
// single MGRS string.
//
/* convert_xastir_to_MGRS_str_components returns each of the components
   of the MGRS string separately.
   Example MGRS string: 18T VK 66790 55998
   Parameters:
   utmZone          Returns the UTM zone: e.g. 18T
   utmZone_len      length of the utmZone char[]
   EastingL         Returns the first letter of the MGRS digraph: e.g. V
   EastingL_len     length of the EastingL char[]
   NorthingL        Returns the second letter of the MGRS digraph: e.g. K
   NorthingL_len    length of the NorthingL char[]
   int_utmEasting   returns the MGRS easting: e.g. 66790
   int_utmNorthing  returns the MGRS northing: e.g. 55998
   x                xastir x coordinate to obtain MGRS coordinate for.
   y                xastir x coordinate to obtain MGRS coordinate for.
   nice_format      1 for populate space_string with three spaces
                    0 to make space_string and empty string, see above.
   space_string     Returned string that can be used to make MGRS strings
                    allign more cleanly with UTM strings.
   space_string_len length of the space_string char[]
*/
void convert_xastir_to_MGRS_str_components(char *utmZone, int UNUSED(utmZone_len),
    char *EastingL, int EastingL_len,
    char *NorthingL, int NorthingL_len,
    unsigned int *int_utmEasting, unsigned int *int_utmNorthing,
    long x,  long y,
    int nice_format, char *space_string, int UNUSED(space_string_len) )
{

  double utmNorthing;
  double utmEasting;
  //char utmZone[10];
  int start;
  int my_east, my_north;
  //unsigned int int_utmEasting, int_utmNorthing;
  int UPS = 0;
  int North_UPS = 0;
  int coordinate_system_save = coordinate_system;
  //char space_string[4] = "   ";    // Three spaces

  // Set for correct zones
  coordinate_system = USE_MGRS;

  ll_to_utm_ups(E_WGS_84,
                (double)(-((y - 32400000l )/360000.0)),
                (double)((x - 64800000l )/360000.0),
                &utmNorthing,
                &utmEasting,
                utmZone,
                sizeof(utmZone) );
  utmZone[9] = '\0';

  // Restore it
  coordinate_system = coordinate_system_save;


  //fprintf(stderr,"%s %07.0f %07.0f\n", utmZone, utmEasting,
  //utmNorthing );
  //xastir_snprintf(str, str_len, "%s %07.0f %07.0f",
  //    utmZone, utmEasting, utmNorthing );


  // Convert the northing and easting values to the digraph letter
  // format.  Letters 'I' and 'O' are skipped for both eastings
  // and northings.  Letters 'W' through 'Z' are skipped for
  // northings.
  //
  // N/S letters alternate in a cycle of two.  Begins at the
  // equator with A and F in alternate zones.  Odd-numbered zones
  // use A-V, even-numbered zones use F-V, then A-V.
  //
  // E/W letters alternate in a cycle of three.  Each E/W zone
  // uses an 8-letter block.  Starts at A-H, then J-R, then S-Z,
  // then repeats every 18 degrees.
  //
  // N/S letters have a cycle of two, E/W letters have a cycle of
  // three.  The same lettering repeats after six zones (2,000,000
  // meter intervals).
  //
  // AA is at equator and 180W.

  // Easting:  Each zone covers 6 degrees.  Zone 1 = A-H, Zone 2 =
  // J-R, Zone 3 = S-Z, then repeat.  So, take zone number-1,
  // modulus 3, multiple that number by 8.  Modulus 24.  That's
  // our starting letter for the zone.  Take the easting number,
  // divide by 100,000, , add the starting number, compute modulus
  // 24, then use that index into our E_W array.
  //
  // Northing:  Figure out whether even/odd zone number.  Divide
  // by 100,000.  If even, add 5 (starts at 'F' if even).  Compute
  // modulus 20, then use that index into our N_S array.


  *int_utmEasting = (unsigned int)utmEasting;
  *int_utmNorthing = (unsigned int)utmNorthing;
  *int_utmEasting = *int_utmEasting % 100000;
  *int_utmNorthing = *int_utmNorthing % 100000;


  // Check for South Polar UPS area, set flags if found.
  if (   utmZone[0] == 'A'
         || utmZone[0] == 'B'
         || utmZone[1] == 'A'
         || utmZone[1] == 'B'
         || utmZone[2] == 'A'
         || utmZone[2] == 'B' )
  {
    // We're in the South Polar UPS area
    UPS++;
  }
  // Check for North Polar UPS area, set flags if found.
  else if (   utmZone[0] == 'Y'
              || utmZone[0] == 'Z'
              || utmZone[1] == 'Y'
              || utmZone[1] == 'Z'
              || utmZone[2] == 'Y'
              || utmZone[2] == 'Z')
  {
    // We're in the North Polar UPS area
    UPS++;
    North_UPS++;
  }
  else
  {
    // We're in the UTM area.  Set no flags.
  }


  if (UPS)    // Special processing for UPS area (A/B/Y/Z bands)
  {

    // Note: Zone number isn't used for UPS, but zone letter is.
    if (nice_format)    // Add two leading spaces
    {
      utmZone[2] = utmZone[0];
      utmZone[0] = ' ';
      utmZone[1] = ' ';
      utmZone[3] = '\0';
    }
    else
    {
      space_string[0] = '\0';
    }

    if (North_UPS)      // North polar UPS zone
    {
      char UPS_N_Easting[15]  = "RSTUXYZABCFGHJ";
      char UPS_N_Northing[15] = "ABCDEFGHJKLMNP";

      // Calculate the index into the 2-letter digraph arrays.
      my_east = (int)(utmEasting / 100000.0);
      my_east = my_east - 13;
      my_north = (int)(utmNorthing / 100000.0);
      my_north = my_north - 13;

      /*xastir_snprintf(str,
          str_len,
          "%s %c%c %05d %s%05d",
          utmZone,
          UPS_N_Easting[my_east],
          UPS_N_Northing[my_north],
          int_utmEasting,
          space_string,
          int_utmNorthing ); */
      xastir_snprintf(EastingL,EastingL_len,
                      "%c", UPS_N_Easting[my_east]);
      xastir_snprintf(NorthingL,NorthingL_len,
                      "%c", UPS_N_Northing[my_north]);
    }
    else    // South polar UPS zone
    {
      char UPS_S_Easting[25]  = "JKLPQRSTUXYZABCFGHJKLPQR";
      char UPS_S_Northing[25] = "ABCDEFGHJKLMNPQRSTUVWXYZ";

      // Calculate the index into the 2-letter digraph arrays.
      my_east = (int)(utmEasting / 100000.0);
      my_east = my_east - 8;
      my_north = (int)(utmNorthing / 100000.0);
      my_north = my_north - 8;

      /* xastir_snprintf(str,
          str_len,
          "%s %c%c %05d %s%05d",
          utmZone,
          UPS_S_Easting[my_east],
          UPS_S_Northing[my_north],
          int_utmEasting,
          space_string,
          int_utmNorthing ); */
      xastir_snprintf(EastingL,EastingL_len,
                      "%c", UPS_S_Easting[my_east]);
      xastir_snprintf(NorthingL,NorthingL_len,
                      "%c", UPS_S_Northing[my_north]);
    }
  }
  else    // UTM Area
  {
    char E_W[25] = "ABCDEFGHJKLMNPQRSTUVWXYZ";
    char N_S[21] = "ABCDEFGHJKLMNPQRSTUV";


    if (!nice_format)
    {
      space_string[0] = '\0';
    }


    // Calculate the indexes into the 2-letter digraph arrays.
    start = atoi(utmZone);
    start--;
    start = start % 3;
    start = start * 8;
    start = start % 24;
    // "start" is now an index into the starting letter for the
    // zone.


    my_east = (int)(utmEasting / 100000.0) - 1;
    my_east = my_east + start;
    my_east = my_east % 24;
//        fprintf(stderr, "Start: %c   East (guess): %c   ",
//            E_W[start],
//            E_W[my_east]);


    start = atoi(utmZone);
    start = start % 2;
    if (start)      // Odd-numbered zone
    {
      start = 0;
    }
    else    // Even-numbered zone
    {
      start = 5;
    }
    my_north = (int)(utmNorthing / 100000.0);
    my_north = my_north + start;
    my_north = my_north % 20;
//        fprintf(stderr, "Start: %c   North (guess): %c\n",
//            N_S[start],
//            N_S[my_north]);

    /* xastir_snprintf(str,
        str_len,
        "%s %c%c %05d %s%05d",
        utmZone,
        E_W[my_east],
        N_S[my_north],
        int_utmEasting,
        space_string,
        int_utmNorthing ); */
    xastir_snprintf(EastingL, EastingL_len,
                    "%c", E_W[my_east]);
    xastir_snprintf(NorthingL, NorthingL_len,
                    "%c", N_S[my_north]);
  }
}





/* Wrapper around convert_xastir_to_MGRS_str_components
   to return an MGRS coordinate as a single string.
   Parameters:
   str The character array to be populated with the MGRS string.
   str_len Length of str.
   x xastir x coordinate.
   y xastir y coordinate.
   If "nice_format" == 1, we add leading spaces plus spaces between
   the easting and northing in order to line up more nicely with the
   UTM output format.
*/
void convert_xastir_to_MGRS_str(char *str, int str_len, long x, long y, int nice_format)
{
  char space_string[4] = "   ";    // Three spaces
  unsigned int intEasting = 0;
  unsigned int intNorthing = 0;
  char EastingL[3] = "  ";
  char NorthingL[3] = "  ";
  char utmZone[10];
  convert_xastir_to_MGRS_str_components(utmZone, strlen(utmZone),
                                        EastingL, sizeof(EastingL),
                                        NorthingL, sizeof(NorthingL),
                                        &intEasting, &intNorthing,
                                        x,  y,
                                        nice_format, space_string, strlen(space_string)) ;
  xastir_snprintf(str,
                  str_len,
                  "%s %c%c %05d %s%05d",
                  utmZone,
                  EastingL[0],
                  NorthingL[0],
                  intEasting,
                  space_string,
                  intNorthing );
}
