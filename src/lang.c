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
#include "lang.h"

// Must be last include file
#include "leak_detection.h"



char lang_code[MAX_LANG_ENTRIES][MAX_LANG_CODE+1];
char *lang_code_ptr[MAX_LANG_ENTRIES];
char lang_buffer[MAX_LANG_BUFFER];
char lang_hotkey[MAX_LANG_ENTRIES];

int lang_code_number;
long buffer_len;
char invalid_code[50];





char *langcode(char *code)
{
  int i;

  // Create an invalid code string to return in case we can't find the proper string
  if (strlen(code) <= MAX_LANG_CODE)      // Code is ok
  {
    xastir_snprintf(invalid_code, sizeof(invalid_code), "IC>%s", code);
  }
  else    // Code is too long
  {
    xastir_snprintf(invalid_code, sizeof(invalid_code), "IC>TOO LONG:%s",code);
    fprintf(stderr,"IC>TOO LONG:%s\n",code);
    return(invalid_code);
  }

  if(lang_code_number>0)
  {
    for(i=0; i<lang_code_number; i++)
    {
      if(strcmp(code,lang_code[i])==0)    // Found a match
      {
        if (strlen(lang_code[i]) < MAX_LANG_LINE_SIZE)
        {
          return(lang_code_ptr[i]);   // Found it, length ok
        }
        else
        {
          fprintf(stderr,"String size: %d,  Max size: %d, %s\n",
                  (int)strlen(lang_code[i]),
                  MAX_LANG_LINE_SIZE,code);
          return(invalid_code);       // Found it, but string too long
        }
      }
    }
    fprintf(stderr,"Language String not found:%s\n",code);
    return(invalid_code);
  }

  fprintf(stderr,"No language strings loaded:%s\n",code);
  return(invalid_code);   // No strings loaded in language file
}





char langcode_hotkey(char *code)
{
  int i;

  if(lang_code_number>0)
  {
    for(i=0; i<lang_code_number; i++)
    {
      if(strcmp(code,lang_code[i])==0)
      {
        return(lang_hotkey[i]); // Found it
      }
    }
  }

  fprintf(stderr,"No hotkey for:%s\n",code);
  return(' ');    // No strings loaded in language file
}





int load_language_file(char *filename)
{
  FILE *f;
  char line[MAX_LANG_LINE_SIZE+1];
  char *temp_ptr;
  int i,lt,lcok;
  char cin;
  int ok;
  int line_num;
  int data_len;

  lang_code_number=0;
  buffer_len=0l;
  ok=1;
  line_num=1;
  i=0;
  f=fopen(filename,"r");
  if(f != NULL)
  {
    line[0]='\0';
    while(!feof(f))
    {
      if(fread(&cin,1,1,f)==1)
      {
        if(cin != (char)10 && cin != (char)13)
        {
          if(i<MAX_LANG_LINE_SIZE)
          {
            line[i++]=cin;
            line[i]='\0';
          }
          else
          {
            ok=0;
            fprintf(stderr,"Error! Line %d too long in language file\n",line_num);
          }
        }
        else
        {
          i=0;
          if (line[0]!='#' && strlen(line)>0)
          {
            /* data line */
            if(lang_code_number < MAX_LANG_ENTRIES)
            {
              if(buffer_len < MAX_LANG_BUFFER)
              {
                if(strchr(line,'|')!=NULL)
                {
                  temp_ptr=strtok(line,"|");            /* get code */
                  if (temp_ptr!=NULL)
                  {
                    if(strlen(temp_ptr)<=MAX_LANG_CODE)
                    {
                      lcok=1;
                      for (lt=0; lt <lang_code_number && lcok; lt++)
                      {
                        if(strcmp(lang_code[lt],temp_ptr)==0)
                        {
                          lcok=0;
                          break;
                        }
                      }
                      if(lcok)
                      {
                        xastir_snprintf(lang_code[lang_code_number],
                                        MAX_LANG_CODE+1,
                                        "%s",
                                        temp_ptr);
                        temp_ptr=strtok(NULL,"|");         /* get string */
                        if (temp_ptr!=NULL)
                        {
                          data_len=(int)strlen(temp_ptr);
                          if ((buffer_len+data_len+1)< MAX_LANG_BUFFER)
                          {
                            lang_code_ptr[lang_code_number]=lang_buffer+buffer_len;
                            xastir_snprintf(lang_buffer+buffer_len,
                                            MAX_LANG_BUFFER-buffer_len,
                                            "%s",
                                            temp_ptr);
                            lang_buffer[buffer_len+data_len]='\0';
                            buffer_len+=data_len+1;
                            temp_ptr=strtok(NULL,"|");      /* get hotkey */
                            if (temp_ptr!=NULL)
                            {
                              lang_hotkey[lang_code_number]=temp_ptr[0];
                              /*fprintf(stderr,"HOTKEY %c\n",lang_hotkey[lang_code_number]);*/
                            }
                          }
                          else
                          {
                            ok=0;
                            fprintf(stderr,"Language data buffer full error on line %d\n",line_num);
                          }
                        }
                        else
                        {
                          ok=0;
                          fprintf(stderr,"Language string parse error on line %d\n",line_num);
                        }
                      }
                      else
                      {
                        ok=0;
                        fprintf(stderr,"Duplicate code! <%s> on line %d\n",temp_ptr,line_num);
                      }
                    }
                    else
                    {
                      ok=0;
                      fprintf(stderr,"Language code on line %d is too long\n",line_num);
                    }
                  }
                  else
                  {
                    ok=0;
                    fprintf(stderr,"Missing Language code data on line %d\n",line_num);
                  }
                }
                else
                {
                  ok=0;
                  fprintf(stderr,"Language code parse error on line %d\n",line_num);
                }
              }
              else
              {
                ok=0;
                fprintf(stderr,"Language data buffer full error on line %d\n",line_num);
              }
            }
            else
            {
              ok=0;
              fprintf(stderr,"Too many Language codes error on line %d\n",line_num);
            }
            if (ok)
            {
              if (debug_level & 32)
                fprintf(stderr,"Code #%d <%s> data <%s> hotkey <%c>\n",lang_code_number,
                        lang_code[lang_code_number],lang_code_ptr[lang_code_number],
                        lang_hotkey[lang_code_number]);
              lang_code_number++;
            }
            line[0]='\0';
          }
          line_num++;
        }
      }
    }
    (void)fclose(f);
  }
  else
  {
    ok=0;
    fprintf(stderr,"Could not read Language file: %s!\n",filename);
  }
  if (debug_level & 32)
  {
    fprintf(stderr,"LANG %d\n",lang_code_number);
  }

  return(ok);
}


