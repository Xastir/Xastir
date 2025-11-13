/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2023 The Xastir Group
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

#include <stdio.h>

int position_amb_chars;

/*************************************************************************/
/* output_lat - format position with position_amb_chars for transmission */
/*************************************************************************/
char *output_lat(char *in_lat, int comp_pos)
{
  int i,j;

//fprintf(stderr,"in_lat:%s\n", in_lat);

  if (!comp_pos)
  {
    // Don't do this as it results in truncation!
    //in_lat[7]=in_lat[8]; // Shift N/S down for transmission
  }
  else if (position_amb_chars>0)
  {
    in_lat[7]='0';
  }

  j=0;
  if (position_amb_chars>0 && position_amb_chars<5)
  {
    for (i=6; i>(6-position_amb_chars-j); i--)
    {
      if (i==4)
      {
        i--;
        j=1;
      }
      if (!comp_pos)
      {
        in_lat[i]=' ';
      }
      else
      {
        in_lat[i]='0';
      }
    }
  }

  if (!comp_pos)
  {
    in_lat[8] = '\0';
  }

  return(in_lat);
}





/**************************************************************************/
/* output_long - format position with position_amb_chars for transmission */
/**************************************************************************/
char *output_long(char *in_long, int comp_pos)
{
  int i,j;

//fprintf(stderr,"in_long:%s\n", in_long);

  if (!comp_pos)
  {
    // Don't do this as it results in truncation!
    //in_long[8]=in_long[9]; // Shift e/w down for transmission
  }
  else if (position_amb_chars>0)
  {
    in_long[8]='0';
  }

  j=0;
  if (position_amb_chars>0 && position_amb_chars<5)
  {
    for (i=7; i>(7-position_amb_chars-j); i--)
    {
      if (i==5)
      {
        i--;
        j=1;
      }
      if (!comp_pos)
      {
        in_long[i]=' ';
      }
      else
      {
        in_long[i]='0';
      }
    }
  }

  if (!comp_pos)
  {
    in_long[9] = '\0';
  }

  return(in_long);
}





