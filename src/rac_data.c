/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2004  The Xastir Group
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

//====================================================================
//    Canadian Callsign Lookup
//    Richard Hagemeyer, VE3UNW
//    GNU COPYLEFT applies
//    1999-11-08
//
//    For use with XASTIR by Frank Giannandrea
//********************************************************************

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else   // TIME_WITH_SYS_TIME
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else  // HAVE_SYS_TIME_H
#  include <time.h>
# endif // HAVE_SYS_TIME_H
#endif  // TIME_WITH_SYS_TIME

#include <Xm/XmAll.h>

#include "rac_data.h"
#include "xa_config.h"
#include "main.h"
#include "xastir.h"





/* ====================================================================    */
/*    my version of chomp from perl, removes spaces and dashes    */
/*                                    */
/* ******************************************************************** */
int chomp(char *input,unsigned int i) {
    unsigned int    x;

    for (x=i;input[x] == ' ' || input[x] == '-';x--)
        input[x] = '\0';

    return ( (int)(i-x) );
}





/* ====================================================================    */
/*    build a new (or newer if I check the file date) index file    */
/*    check for current ic index file                    */
/*      FG: added a date check in case the RAC file has been updated.   */
/*      AMACALL.LST must have a time stamp newer than the index file    */
/*      time stamp. Use the touch command on the AMACALL.LST file to    */
/*      make the time current if necessary.                             */
/* ******************************************************************** */
int build_rac_index(void) {
    FILE *fdb;
    FILE *fndx;
    unsigned long call_offset = 0;
    unsigned long x = 0;
    char racdata[RAC_DATA_LEN+8];

    /* ====================================================================    */
    /*    If the index file is there, exit                */
    /*                                    */
    if (filethere(get_user_base_dir("data/AMACALL.ndx"))) {
        /* if file is there make sure the index date is newer */
        if(file_time(get_data_base_dir("fcc/AMACALL.LST"))<=file_time(get_user_base_dir("data/AMACALL.ndx"))) {
            return(1);
        } else {
            statusline("RAC Index old rebuilding",1);
//            XmTextFieldSetString(text,"RAC Index old rebuilding");
//            XtManageChild(text);
//            XmUpdateDisplay(XtParent(text));
        }
    }
    /* ====================================================================    */
    /*    Open the database and index file                */
    /*                                    */
    fdb=fopen(get_data_base_dir("fcc/AMACALL.LST"),"rb");
    if (fdb==NULL) {
        fprintf(stderr,"Build:Could not open RAC data base: %s\n", get_data_base_dir("fcc/AMACALL.LST") );
        return(0);
    }

    fndx=fopen(get_user_base_dir("data/AMACALL.ndx"),"w");
    if (fndx==NULL) {
        fprintf(stderr,"Build:Could not open/create RAC data base index: %s\n", get_user_base_dir("data/AMACALL.ndx") );
        (void)fclose(fdb);
        return(0);
    }
    /* ====================================================================    */
    /*    Skip past the header to the first callsign (VA2AA)        */
    /*                                    */
    while (!feof(fdb) && strncmp(racdata,"VA",2)) {
        call_offset = (unsigned long)ftell(fdb);
        if (fgets(racdata, (int)sizeof(racdata), fdb)==NULL) {
            fprintf(stderr,"Build:header:Unable to read data base\n");
            (void)fclose(fdb);
            (void)fclose(fndx);
            fprintf(stderr,"rc=0\n");
            return (0);
        }
    }

    /* ====================================================================    */
    /*    write out the current callsign and RBA of the db file         */
    /*    skip 100 records and do it again until no more            */
    /*                                    */
    while (!feof(fdb)) {
        fprintf(fndx,"%6.6s%li\n",racdata,(long)call_offset);
        call_offset = (unsigned long)ftell(fdb);
        for (x=0;x<=100 && !feof(fdb);x++)
            if (fgets(racdata, (int)sizeof(racdata), fdb)==NULL)
                break;
    }
    (void)fclose(fdb);
    (void)fclose(fndx);

//    XmTextFieldSetString(text,"");
//    XtManageChild(text);
    return(1);
}





/* ====================================================================    */
/*    Check for ic data base file                    */
/*    Check/build the index                        */
/*                                    */
/* ******************************************************************** */
int check_rac_data(void) {
    int rac_data_available = 0;
    if( filethere( get_data_base_dir("fcc/AMACALL.LST") ) ) {
        if (build_rac_index())
            rac_data_available=1;
        else {
            fprintf(stderr,"Check:Could not build ic data base index\n");
            rac_data_available=0;
        }
    }
    return(rac_data_available);
}





/* ====================================================================    */
/*    The real work.  Pass the callsign, get the info            */
/*                                    */
/* ******************************************************************** */
int search_rac_data(char *callsign, rac_record *data) {
    FILE *fdb;
    FILE *fndx;
    char *rc;
    long call_offset = 0;
    char char_offset[16];
    char index[32];
    int found = 0;
    rac_record racdata;
    /*char        filler[8];*/

    strncpy(index," ",16);
    strncpy(racdata.callsign," ",sizeof(racdata));

    /* ====================================================================    */
    /*    Search thru the index, get the RBA                */
    /*                                    */
    fndx=fopen(get_user_base_dir("data/AMACALL.ndx"),"r");
    if(fndx!=NULL) {
        rc = fgets(index,(int)sizeof(index),fndx);
        strncpy(char_offset,&index[6],16);
        while (!feof(fndx) && strncmp(callsign,index,6) > 0) {
            strncpy(char_offset,&index[6],16);
            rc = fgets(index,(int)sizeof(index),fndx);
        }
    } else {
        fprintf(stderr,"Search:Could not open RAC data base index: %s\n", get_user_base_dir("data/AMACALL.ndx") );
        return (0);
    }
    call_offset = atol(char_offset);
    (void)fclose(fndx);

    /* ====================================================================    */
    /*    Now we have our pointer into the main database (text) file.    */
    /*    Start from there and linear search the data.            */
    /*    This will take an avg of 1/2 of the # skipped making the index    */
    /*                                    */

    fdb=fopen(get_data_base_dir("fcc/AMACALL.LST"),"r");
    if (fdb!=NULL) {
        (void)fseek(fdb, call_offset,SEEK_SET);
        if (callsign[5] == '-')
            (void)chomp(callsign,5);

        while (!feof(fdb) && strncmp((char *)&racdata,callsign,6) < 0)

//WE7U
// Problem here:  We're sticking 8 bytes too many into racdata!
            rc = fgets((char *)&racdata, sizeof(racdata), fdb);

    } else
        fprintf(stderr,"Search:Could not open RAC data base: %s\n", get_data_base_dir("fcc/AMACALL.LST") );

    /*  || (callsign[5] == '-' && strncmp((char *)&racdata,callsign,5) < 0)) */
    (void)chomp(racdata.callsign,6);
    if (!strncmp((char *)racdata.callsign,callsign,6)) {
        found = 1;
        (void)chomp(racdata.first_name,35);
        (void)chomp(racdata.last_name,35);
        (void)chomp(racdata.address,70);
        (void)chomp(racdata.city,35);
        (void)chomp(racdata.province,2);
        (void)chomp(racdata.postal_code,10);
        (void)chomp(racdata.qual_a,1);
        (void)chomp(racdata.qual_b,1);
        (void)chomp(racdata.qual_c,1);
        (void)chomp(racdata.qual_d,1);
        (void)chomp(racdata.club_name,141);
        (void)chomp(racdata.club_address,70);
        (void)chomp(racdata.club_city,35);
        (void)chomp(racdata.club_province,2);
        (void)chomp(racdata.club_postal_code,9);
        strcpy(data->callsign,racdata.callsign);
        strcpy(data->first_name,racdata.first_name);
        strcpy(data->last_name,racdata.last_name);
        strcpy(data->address,racdata.address);
        strcpy(data->city,racdata.city);
        strcpy(data->province,racdata.province);
        strcpy(data->postal_code,racdata.postal_code);
        strcpy(data->qual_a,racdata.qual_a);
        strcpy(data->qual_b,racdata.qual_b);
        strcpy(data->qual_c,racdata.qual_c);
        strcpy(data->qual_d,racdata.qual_d);
        strcpy(data->club_name,racdata.club_name);
        strcpy(data->club_address,racdata.club_address);
        strcpy(data->club_city,racdata.club_city);
        strcpy(data->club_province,racdata.club_province);
        strcpy(data->club_postal_code,racdata.club_postal_code);
    }
    (void)fclose(fdb);

    if (!found) {
        popup_message("Callsign Search", "Callsign Not Found!");
    }

    return(found);
}


