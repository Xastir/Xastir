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
  #include <sys/time.h>
  #include <time.h>
#else   // TIME_WITH_SYS_TIME
  #if HAVE_SYS_TIME_H
    #include <sys/time.h>
  #else  // HAVE_SYS_TIME_H
    #include <time.h>
  #endif // HAVE_SYS_TIME_H
#endif  // TIME_WITH_SYS_TIME

#include <Xm/XmAll.h>

#include "xastir.h"
#include "rac_data.h"
#include "xa_config.h"
#include "main.h"
#include "snprintf.h"

// Must be last include file
#include "leak_detection.h"





/* ====================================================================    */
/*    my version of chomp from perl, removes spaces and dashes    */
/*                                    */
/* ******************************************************************** */
int chomp(char *input, unsigned int i)
{
  unsigned int    x;

  for (x=i; input[x] == ' ' || input[x] == '-'; x--)
  {
    input[x] = '\0';
  }

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
int build_rac_index(void)
{
  FILE *fdb;
  FILE *fndx;
  unsigned long call_offset = 0;
  unsigned long x = 0;
  char racdata[RAC_DATA_LEN+8];
  char amacall_path[MAX_VALUE];

  get_user_base_dir("data/AMACALL.ndx", amacall_path, sizeof(amacall_path));

  /* ====================================================================    */
  /*    If the index file is there, exit                */
  /*                                    */
  if (filethere(amacall_path))
  {
    /* if file is there make sure the index date is newer */
    if(file_time(amacall_path)<=file_time(amacall_path))
    {
      return(1);
    }
    else
    {

      // RAC index old, rebuilding
      statusline(langcode("STIFCC0103"), 1);

      fprintf(stderr,"RAC index is old.  Rebuilding index.\n");
//            XmTextFieldSetString(text,"RAC Index old rebuilding");
//            XtManageChild(text);
//            XmUpdateDisplay(XtParent(text));
    }
  }
  /* ====================================================================    */
  /*    Open the database and index file                */
  /*                                    */
  fdb=fopen(get_data_base_dir("fcc/AMACALL.LST"),"rb");
  if (fdb==NULL)
  {
    fprintf(stderr,"Build:Could not open RAC data base: %s\n", get_data_base_dir("fcc/AMACALL.LST") );
    return(0);
  }

  fndx=fopen(amacall_path,"w");
  if (fndx==NULL)
  {
    fprintf(stderr,"Build:Could not open/create RAC data base index: %s\n", amacall_path );
    (void)fclose(fdb);
    return(0);
  }
  /* ====================================================================    */
  /*    Skip past the header to the first callsign (VA2AA)        */
  /*                                    */
  memset(racdata, 0, sizeof(racdata));
  while (!feof(fdb) && strncmp(racdata,"VA",2))
  {
    call_offset = (unsigned long)ftell(fdb);
    if (fgets(racdata, (int)sizeof(racdata), fdb)==NULL)
    {
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
  while (!feof(fdb))
  {
    fprintf(fndx,"%6.6s%li\n",racdata,(long)call_offset);
    call_offset = (unsigned long)ftell(fdb);
    for (x=0; x<=100 && !feof(fdb); x++)
      if (fgets(racdata, (int)sizeof(racdata), fdb)==NULL)
      {
        break;
      }
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
int check_rac_data(void)
{
  int rac_data_available = 0;
  if( filethere( get_data_base_dir("fcc/AMACALL.LST") ) )
  {
    if (build_rac_index())
    {
      rac_data_available=1;
    }
    else
    {
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
int search_rac_data(char *callsign, rac_record *data)
{
  FILE *fdb;
  FILE *fndx;
  long call_offset = 0l;
  char char_offset[16];
  char index[32];
  int found = 0;
  rac_record racdata;
  /*char        filler[8];*/
  char amacall_path[MAX_VALUE];

  get_user_base_dir("data/AMACALL.ndx", amacall_path, sizeof(amacall_path));


  xastir_snprintf(index, sizeof(index)," ");
  xastir_snprintf(racdata.callsign, sizeof(racdata.callsign)," ");

  /* ====================================================================    */
  /*    Search thru the index, get the RBA                */
  /*                                    */

  fndx = fopen(amacall_path, "r");
  if(fndx != NULL)
  {
    if (fgets(index, (int)sizeof(index), fndx) == NULL)
    {
      // Error occurred
      fprintf(stderr,
              "Search:Could not read RAC data base index: %s\n",
              amacall_path );
      return (0);
    }
    memcpy(char_offset, &index[6], sizeof(char_offset));
    char_offset[sizeof(char_offset)-1] = '\0';  // Terminate string
    while (!feof(fndx) && strncmp(callsign, index, 6) > 0)
    {
      memcpy(char_offset, &index[6], sizeof(char_offset));
      char_offset[sizeof(char_offset)-1] = '\0';  // Terminate string
      if (fgets(index, (int)sizeof(index), fndx) == NULL)
      {
        // Error occurred
        fprintf(stderr,
                "Search:Could not read RAC data base index(2): %s\n",
                amacall_path );
        return (0);
      }
    }
  }
  else
  {
    fprintf(stderr,
            "Search:Could not open RAC data base index: %s\n",
            amacall_path );
    return (0);
  }
  call_offset = atol(char_offset);

  (void)fclose(fndx);

  /* ====================================================================    */
  /*    Now we have our pointer into the main database (text) file.    */
  /*    Start from there and linear search the data.            */
  /*    This will take an avg of 1/2 of the # skipped making the index    */
  /*                                    */

  fdb = fopen(get_data_base_dir("fcc/AMACALL.LST"), "r");
  if (fdb != NULL)
  {
    (void)fseek(fdb, call_offset, SEEK_SET);
    if (callsign[5] == '-')
    {
      (void)chomp(callsign,5);
    }

    while (!feof(fdb) && strncmp((char *)&racdata, callsign, 6) < 0)

//WE7U
// Problem here:  We're sticking 8 bytes too many into racdata!
      if (fgets((char *)&racdata, sizeof(racdata), fdb) == NULL)
      {
        // Error occurred
        fprintf(stderr,
                "Search:Could not read RAC data base: %s\n",
                amacall_path );
        return (0);
      }

  }
  else
  {
    fprintf(stderr,"Search:Could not open RAC data base: %s\n", get_data_base_dir("fcc/AMACALL.LST") );
  }

  /*  || (callsign[5] == '-' && strncmp((char *)&racdata,callsign,5) < 0)) */
  (void)chomp(racdata.callsign, 6);

  if (!strncmp((char *)racdata.callsign, callsign, 6))
  {
    found = 1;

// Some of these cause problems on 64-bit processors, so commented
// them out for now.
//        (void)chomp(racdata.first_name, 35);
//        (void)chomp(racdata.last_name, 35);
//        (void)chomp(racdata.address, 70);
//        (void)chomp(racdata.city, 35);
//        (void)chomp(racdata.province, 2);
//        (void)chomp(racdata.postal_code, 10);
//        (void)chomp(racdata.qual_a, 1);
//        (void)chomp(racdata.qual_b, 1);
//        (void)chomp(racdata.qual_c, 1);
//        (void)chomp(racdata.qual_d, 1);
//        (void)chomp(racdata.club_name, 141);
//        (void)chomp(racdata.club_address, 70);
//        (void)chomp(racdata.club_city, 35);
//        (void)chomp(racdata.club_province, 2);
//        (void)chomp(racdata.club_postal_code, 9);

    xastir_snprintf(data->callsign,
                    sizeof(data->callsign),
                    "%s",
                    racdata.callsign);

    xastir_snprintf(data->first_name,
                    sizeof(data->first_name),
                    "%s",
                    racdata.first_name);

    xastir_snprintf(data->last_name,
                    sizeof(data->last_name),
                    "%s",
                    racdata.last_name);

    xastir_snprintf(data->address,
                    sizeof(data->address),
                    "%s",
                    racdata.address);

    xastir_snprintf(data->city,
                    sizeof(data->city),
                    "%s",
                    racdata.city);

    xastir_snprintf(data->province,
                    sizeof(data->province),
                    "%s",
                    racdata.province);

    xastir_snprintf(data->postal_code,
                    sizeof(data->postal_code),
                    "%s",
                    racdata.postal_code);

    xastir_snprintf(data->qual_a,
                    sizeof(data->qual_a),
                    "%s",
                    racdata.qual_a);

    xastir_snprintf(data->qual_b,
                    sizeof(data->qual_b),
                    "%s",
                    racdata.qual_b);

    xastir_snprintf(data->qual_c,
                    sizeof(data->qual_c),
                    "%s",
                    racdata.qual_c);

    xastir_snprintf(data->qual_d,
                    sizeof(data->qual_d),
                    "%s",
                    racdata.qual_d);

    xastir_snprintf(data->club_name,
                    sizeof(data->club_name),
                    "%s",
                    racdata.club_name);

    xastir_snprintf(data->club_address,
                    sizeof(data->club_address),
                    "%s",
                    racdata.club_address);

    xastir_snprintf(data->club_city,
                    sizeof(data->club_city),
                    "%s",
                    racdata.club_city);

    xastir_snprintf(data->club_province,
                    sizeof(data->club_province),
                    "%s",
                    racdata.club_province);

    xastir_snprintf(data->club_postal_code,
                    sizeof(data->club_postal_code),
                    "%s",
                    racdata.club_postal_code);

  }
  (void)fclose(fdb);

  if (!found)
  {

    // "Callsign Search", "Callsign Not Found!"
    popup_message_always(langcode("STIFCC0101"),
                         langcode("STIFCC0102") );
  }

  return(found);
}


