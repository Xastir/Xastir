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
#include <string.h>
#include <ctype.h>

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
#include "fcc_data.h"
#include "xa_config.h"
#include "main.h"

// Must be last include file
#include "leak_detection.h"





char *call_only(char *callsign)
{
  int i, len;

  len = strlen(callsign);
  for (i = 0; i < len; i++)
  {
    if (!isalnum((int)callsign[i]))
    {
      callsign[i]='\0';
      i=len;
    }
  }
  return(callsign);
}





/* ====================================================================  */
/*    build a new (or newer if I check the file date) index file     */
/*    check for current ic index file                     */
/*    FG: added a date check in case the FCC file has been updated.    */
/*      appl.dat must have a time stamp newer than the index file time  */
/*      stamp. Use the touch command on the appl.dat file to make the   */
/*      time current if necessary.                                      */
//    How this works:  The index file contains a few callsigns and their
//    offsets into the large database file.  The code uses these as
//    jump-off points to look for a particular call, to speed things up.
/* ******************************************************************** */
int build_fcc_index(int type)
{
  FILE *fdb;
  FILE *fndx;
  unsigned long call_offset = 0;
  unsigned long x = 0;
  char fccdata[FCC_DATA_LEN+8];
  char database_name[100];
  int found,i,num;
  char appl_file_path[MAX_VALUE];

  if (type==1)
  {
    xastir_snprintf(database_name, sizeof(database_name), "fcc/appl.dat");
  }
  else
  {
    xastir_snprintf(database_name, sizeof(database_name), "fcc/EN.dat");
  }

  /* ====================================================================    */
  /*    If the index file is there, exit                */
  /*                                    */
  get_user_base_dir("data/appl.ndx", appl_file_path,
                    sizeof(appl_file_path));
  if (filethere(appl_file_path))
  {
    /* if file is there make sure the index date is newer */
    if (file_time(get_data_base_dir(database_name))<=file_time(appl_file_path))
    {
      return(1);
    }
    else
    {
      // FCC index old, rebuilding
      statusline(langcode("STIFCC0100"),1);

      fprintf(stderr,"FCC index is old.  Rebuilding index.\n");
//            XmTextFieldSetString(text,"FCC index old, rebuilding");
//            XtManageChild(text);
//            XmUpdateDisplay(XtParent(text));     // DK7IN: do we need this ???
    }
  }

  /* ====================================================================    */
  /*    Open the database and index file                */
  /*                                    */
  fdb=fopen(get_data_base_dir(database_name),"rb");
  if (fdb==NULL)
  {
    fprintf(stderr,"Build:Could not open FCC data base: %s\n", get_data_base_dir(database_name) );
    return(0);
  }

  fndx=fopen(appl_file_path,"w");
  if (fndx==NULL)
  {
    fprintf(stderr,"Build:Could not open/create FCC data base index: %s\n", appl_file_path );
    (void)fclose(fdb);
    return(0);
  }

  /* ====================================================================    */
  /*    write out the current callsign and RBA of the db file         */
  /*    skip (index_skip) records and do it again until no more        */
  /*                                    */
  xastir_snprintf(fccdata,sizeof(fccdata)," ");
  while(!feof(fdb))
  {
    call_offset = (unsigned long)ftell(fdb);
    if (fgets(fccdata, (int)sizeof(fccdata), fdb) == NULL)
    {
      // error occurred
      fprintf(stderr,"Build: Could not read fcc data base: %s \n", appl_file_path);
      (void)fclose(fdb);
      return(0);
    }
    found=0;
    num=0;
    if (type==2)
    {
      for(i=0; i<14 && !found; i++)
      {
        if(fccdata[i]=='|')
        {
          num++;
          if(num==4)
          {
            found=i+1;
          }
        }
      }
    }
    (void)call_only(fccdata+found);
    fprintf(fndx,"%-6.6s%li\n",fccdata+found,call_offset+found);
    for (x=0; x<=500 && !feof(fdb); x++)
    {
      if (fgets(fccdata, (int)sizeof(fccdata), fdb)==NULL)
      {
        break;
      }
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
int check_fcc_data(void)
{
  int fcc_data_available = 0;
  if (filethere(get_data_base_dir("fcc/EN.dat")) && filethere(get_data_base_dir("fcc/appl.dat")))
  {
    if(file_time(get_data_base_dir("fcc/appl.dat"))<=file_time(get_data_base_dir("fcc/EN.dat")))
    {
      /*fprintf(stderr,"NEW FORMAT FCC DATA FILE is NEWER THAN OLD FCC FORMAT\n");*/
      if (build_fcc_index(2))
      {
        fcc_data_available=2;
      }
      else
      {
        fprintf(stderr,"Check:Could not build fcc data base index\n");
        fcc_data_available=0;
      }
    }
    else
    {
      /*fprintf(stderr,"OLD FORMAT FCC DATA FILE is NEWER THAN NEW FCC FORMAT\n");*/
      if (build_fcc_index(1))
      {
        fcc_data_available=1;
      }
      else
      {
        fprintf(stderr,"Check:Could not build fcc data base index\n");
        fcc_data_available=0;
      }
    }
  }
  else
  {
    if (filethere(get_data_base_dir("fcc/EN.dat")))
    {
      /*fprintf(stderr,"NO OLD FCC, BUT NEW FORMAT FCC DATA AVAILABLE\n");*/
      if (build_fcc_index(2))
      {
        fcc_data_available=2;
      }
      else
      {
        fprintf(stderr,"Check:Could not build fcc data base index\n");
        fcc_data_available=0;
      }
    }
    else
    {
      if (filethere(get_data_base_dir("fcc/appl.dat")))
      {
        /*fprintf(stderr,"NO NEW FCC, BUT OLD FORMAT FCC DATA AVAILABLE\n");*/
        if (build_fcc_index(1))
        {
          fcc_data_available=1;
        }
        else
        {
          fprintf(stderr,"Check:Could not build fcc data base index\n");
          fcc_data_available=0;
        }
      }
    }
  }
  return(fcc_data_available);
}





int search_fcc_data_appl(char *callsign, FccAppl *data)
{
  FILE *f;
  char line[200];
  int line_pos;
  char data_in[16385];
  int found, xx, bytes_read;
  char temp[15];
  int len;
  int which;
  int i,ii;
  int pos_it;
  int llen;
  char calltemp[8];
  int pos,ix,num;
  FILE *fndx;
  long call_offset = 0;
  char char_offset[16];
  char index[32];
  char appl_file_path[MAX_VALUE];

  data->id_file_num[0] = '\0';
  data->type_purpose[0] = '\0';
  data->type_applicant=' ';
  data->name_licensee[0] = '\0';
  data->text_street[0] = '\0';
  data->text_pobox[0] = '\0';
  data->city[0] = '\0';
  data->state[0] = '\0';
  data->zipcode[0] = '\0';
  data->date_issue[0] = '\0';
  data->date_expire[0] = '\0';
  data->date_last_change[0] = '\0';
  data->id_examiner[0] = '\0';
  data->renewal_notice=' ';
  xastir_snprintf(temp,
                  sizeof(temp),
                  "%s",
                  callsign);
  (void)call_only(temp);

  xastir_snprintf(calltemp, sizeof(calltemp), "%-6.6s", temp);
// calltemp doesn't appear to get used anywhere...

  /* add end of field data */
  strncat(temp, "|", sizeof(temp) - 1 - strlen(temp));
  len=(int)strlen(temp);
  found=0;
  line_pos=0;
  /* check the database again */
  which = check_fcc_data();

  // Check for first letter of a U.S. callsign
  if (! (callsign[0] == 'A' || callsign[0] == 'K' || callsign[0] == 'N' || callsign[0] == 'W') )
  {
    return(0);  // Not found
  }

  // ====================================================================
  // Search thru the index, get the RBA
  //
  // This gives us a jumping-off point to start looking in the right
  // neighborhood for the callsign of interest.
  //
  get_user_base_dir("data/appl.ndx", appl_file_path, sizeof(appl_file_path));
  fndx=fopen(appl_file_path,"r");
  if (fndx!=NULL)
  {
    if (fgets(index,(int)sizeof(index),fndx) == NULL)
    {
      // Error occurred
      fprintf(stderr,"Search:Could not read FCC database index(1): %s\n",appl_file_path);
      return(0);
    }

    memcpy(char_offset, &index[6], sizeof(char_offset));
    char_offset[sizeof(char_offset)-1] = '\0';  // Terminate string

    // Search through the indexes looking for a callsign which is
    // close to the callsign of interest.  If callsign is later in
    // the alphabet than the current index, snag the next index.
    while (!feof(fndx) && strncmp(callsign,index,6) > 0)
    {
      memcpy(char_offset, &index[6], sizeof(char_offset));
      char_offset[sizeof(char_offset)-1] = '\0';  // Terminate string
      if (fgets(index,(int)sizeof(index),fndx) == NULL)
      {
        // Error occurred
        fprintf(stderr,"Search:Could not read FCC database index(2): %s\n", appl_file_path );
        return(0);
      }
    }
  }
  else
  {
    fprintf(stderr,"Search:Could not open FCC data base index: %s\n", appl_file_path );
    return (0);
  }
  call_offset = atol(char_offset);

  (void)fclose(fndx);

  /* ====================================================================    */
  /*    Continue with the original search                */
  /*                                                                */

  f=NULL;
  switch (which)
  {
    case(1):
      f=fopen(get_data_base_dir("fcc/appl.dat"),"r");
      break;

    case(2):
      f=fopen(get_data_base_dir("fcc/EN.dat"),"r");
      break;

    default:
      break;
  }
  if (f!=NULL)
  {
    (void)fseek(f, call_offset,SEEK_SET);
    while (!feof(f) && !found)
    {
      bytes_read=(int)fread(data_in,1,16384,f);
      if (bytes_read>0)
      {
        for (xx=0; (xx<bytes_read) && !found; xx++)
        {
          if(data_in[xx]!='\n' && data_in[xx]!='\r')
          {
            if (line_pos<199)
            {
              line[line_pos++]=data_in[xx];
              line[line_pos]='\0';
            }
          }
          else
          {
            line_pos=0;
            /*fprintf(stderr,"line:%s\n",line);*/
            pos=0;
            num=0;
            if (which==2)
            {
              for (ix=0; ix<14 && !pos; ix++)
              {
                if (line[ix]=='|')
                {
                  num++;
                  if (num==4)
                  {
                    pos=ix+1;
                  }
                }
              }
            }
            if (strncmp(line+pos,temp,(size_t)len)==0)
            {
              found=1;
              /*fprintf(stderr,"line:%s\n",line);*/
              llen=(int)strlen(line);
              /* replace "|" with 0 */
              for (ii=pos; ii<llen; ii++)
              {
                if (line[ii]=='|')
                {
                  line[ii]='\0';
                }
              }
              pos_it=pos;
              for (i=0; i<15; i++)
              {
                for (ii=pos_it; ii<llen; ii++)
                {
                  if (line[ii]=='\0')
                  {
                    pos_it=ii;
                    ii=llen+1;
                  }
                }
                pos_it++;
                if (line[pos_it]!='\0')
                {
                  /*fprintf(stderr,"DATA %d %d:%s\n",i,pos_it,line+pos_it);*/
                  switch (which)
                  {
                    case(1):
                      switch(i)
                      {
                        case(0):
                          xastir_snprintf(data->id_file_num,sizeof(data->id_file_num),"%s",line+pos_it);
                          break;

                        case(1):
                          xastir_snprintf(data->type_purpose,sizeof(data->type_purpose),"%s",line+pos_it);
                          break;

                        case(2):
                          data->type_applicant=line[pos_it];
                          break;

                        case(3):
                          xastir_snprintf(data->name_licensee,sizeof(data->name_licensee),"%s",line+pos_it);
                          break;

                        case(4):
                          xastir_snprintf(data->text_street,sizeof(data->text_street),"%s",line+pos_it);
                          break;

                        case(5):
                          xastir_snprintf(data->text_pobox,sizeof(data->text_pobox),"%s",line+pos_it);
                          break;

                        case(6):
                          xastir_snprintf(data->city,sizeof(data->city),"%s",line+pos_it);
                          break;

                        case(7):
                          xastir_snprintf(data->state,sizeof(data->state),"%s",line+pos_it);
                          break;

                        case(8):
                          xastir_snprintf(data->zipcode,sizeof(data->zipcode),"%s",line+pos_it);
                          break;

                        case(9):
                          xastir_snprintf(data->date_issue,sizeof(data->date_issue),"%s",line+pos_it);
                          break;

                        case(11):
                          xastir_snprintf(data->date_expire,sizeof(data->date_expire),"%s",line+pos_it);
                          break;

                        case(12):
                          xastir_snprintf(data->date_last_change,sizeof(data->date_last_change),"%s",line+pos_it);
                          break;

                        case(13):
                          xastir_snprintf(data->id_examiner,sizeof(data->id_examiner),"%s",line+pos_it);
                          break;

                        case(14):
                          data->renewal_notice=line[pos_it];
                          break;

                        default:
                          break;
                      }
                      break;

                    case(2):
                      switch (i)
                      {
                        case(0):
                          xastir_snprintf(data->id_file_num,sizeof(data->id_file_num),"%s",line+pos_it);
                          break;

                        case(2):
                          xastir_snprintf(data->name_licensee,sizeof(data->name_licensee),"%s",line+pos_it);
                          break;

                        case(10):
                          xastir_snprintf(data->text_street,sizeof(data->text_street),"%s",line+pos_it);
                          break;

                        case(11):
                          xastir_snprintf(data->city,sizeof(data->city),"%s",line+pos_it);
                          break;

                        case(12):
                          xastir_snprintf(data->state,sizeof(data->state),"%s",line+pos_it);
                          break;

                        case(13):
                          xastir_snprintf(data->zipcode,sizeof(data->zipcode),"%s",line+pos_it);
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
            }
            else
            {
              // Check whether we passed the alphabetic
              // location for the callsign.  Return if so.
              if ( (temp[0] < line[pos]) ||
                   ( (temp[0] == line[pos]) && (temp[1] < line[pos+1]) ) )
              {

                // "Callsign Search", "Callsign Not Found!"
                popup_message_always(langcode("STIFCC0101"),
                                     langcode("STIFCC0102") );

                //fprintf(stderr,"%c%c\t%c%c\n",temp[0],temp[1],line[pos],line[pos+1]);
                (void)fclose(f);
                return(0);
              }
            }
          }
        }
      }
    }
    (void)fclose(f);
  }
  else
  {
    fprintf(stderr,"Could not open FCC appl data base at: %s\n", get_data_base_dir("fcc/") );
  }
  return(found);
}


