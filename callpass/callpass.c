/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2007  The Xastir Group
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



static short doHash(char *theCall) {
    char rootCall[10]; // need to copy call to remove ssid from parse
    char *p1 = rootCall;
    short hash;
    short i,len;
    char *ptr = rootCall;

    while ((*theCall != '-') && (*theCall != '\0')) *p1++ = toupper((int)(*theCall++));
        *p1 = '\0';

    hash = kKey; // Initialize with the key value
    i = 0;
    len = (short)strlen(rootCall);

    while (i<len) {// Loop through the string two bytes at a time
        hash ^= (unsigned char)(*ptr++)<<8; // xor high byte with accumulated hash
        hash ^= (*ptr++); // xor low byte with accumulated hash
        i += 2;
    }
    return (short)(hash & 0x7fff); // mask off the high bit so number is always positive
}



short checkHash(char *theCall, short theHash) {
    return (short)(doHash(theCall) == theHash);
}



int main(int argc, char *argv[]) {
    char temp[100];

    if (argc>1) {

        strncpy(temp,argv[1],100);

        temp[99] = '\0';    // Forced termination

        if (strlen(temp)>0)
            printf("Passcode for %s is %d\n",temp,doHash(temp));

    } else
        printf("Usage:callpass <callsign>\n");

    return(0);
}

