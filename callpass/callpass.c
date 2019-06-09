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

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define kKey 0x73e2 // This is the seed for the key
#define true 1





static short doHash(char *theCall)
{
  char rootCall[11];    // need to copy call to remove ssid from parse
  char *p1 = rootCall;
  short hash;
  short i, len = 0;
  char *ptr = rootCall;

  while ( (*theCall != '-') && (*theCall != '\0') )
  {
    *p1++ = toupper( (int)(*theCall++) );
    len++;
  }
  *p1 = '\0';

  hash = kKey;    // Initialize with the key value
  i = 0;

  while (i < len)   // Loop through the string two bytes at a time
  {
    hash ^= (unsigned char)(*ptr++) << 8;   // xor high byte with accumulated hash
    hash ^= (*ptr++);   // xor low byte with accumulated hash
    i += 2;
  }

  return (short)(hash & 0x7fff);  // mask off the high bit so number is always positive
}





short checkHash(char *theCall, short theHash)
{
  return (short)(doHash(theCall) == theHash);
}





int main(int argc, char *argv[])
{
  char temp[11];

  if (argc>1)
  {
    memmove(temp, argv[1], 11); // Process up to 10 characters
    temp[10] = '\0';    // Forced string terminator

    if (temp[0] != '\0')
    {
      printf("Passcode for %s is %d\n", temp, doHash(temp));
    }

  }
  else
  {
    printf("Usage:callpass <callsign>\n");
  }

  return(0);
}


