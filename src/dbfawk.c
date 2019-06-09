/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
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
 */
/*
 * This library glues the Awk-like functions (see awk.c) to attributes
 * for shapefiles (contained in DBF files).
 *
 * Alan Crosswell, n2ygk@weca.org
 *
 */


//
// Functions which allocate memory:
// --------------------------------
// dbfawk_field_list
// dbfawk_load_sigs
// dbfawk_find_sig
// dbfawk_parse_record (indirectly)
//
// Functions which free memory:
// ----------------------------
// dbfawk_free_info
// dbfawk_load_sigs
// dbfawk_free_sig
// dbfawk_free_sigs
// dbfawk_find_sig
//


#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#if defined(WITH_DBFAWK) && defined(HAVE_LIBSHP) && defined(HAVE_LIBPCRE)
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include "awk.h"
#include "dbfawk.h"
#include "snprintf.h"
#include "maps.h"

#include <sys/stat.h>

// Must be last include file
#include "leak_detection.h"





/*
 * dbfawk_sig:  Generate a signature for a DBF file.
 *  Fills in sig and returns number of fields.
 */
int dbfawk_sig(DBFHandle dbf, char *sig, int size)
{
  int nf = 0;

  if (sig && size > 0 && dbf)
  {
    int i;
    char *sp;
    int width,prec;

    nf = DBFGetFieldCount(dbf);
    for (i = 0, sp=sig; sp < &sig[size-XBASE_FLDHDR_SZ] && i < nf ; i++)
    {
      DBFGetFieldInfo(dbf,i,sp,&width,&prec);
      sp += strlen(sp);
      *sp++ = ':';              /* field name separator */
    }
    if (i)
    {
      *--sp = '\0';  /* clobber the trailing sep */
    }
  }
  return nf;
}





/* Free a field list */
void dbfawk_free_info ( dbfawk_field_info *list)
{
  dbfawk_field_info *x, *p;

  for ( p = list; p != NULL; )
  {
    x = p;
    p = p->next;
    free(x);
  }
}





/*
 * dbfawk_field_list: Generate a list of info about fields to read for
 *  a given DBFHandle and colon-separated list of fieldnames.
 */
dbfawk_field_info *dbfawk_field_list(DBFHandle dbf, char *dbffields)
{
  dbfawk_field_info *fi = NULL, *head = NULL, *prev;
  int nf;
  char *sp;

  /* now build up the list of fields to read */
  for (nf = 0, sp = dbffields, prev = NULL; *sp; nf++)
  {
    char *d,*p = sp;
    char junk[XBASE_FLDHDR_SZ];
    int w,prec;

    fi = calloc(1,sizeof(dbfawk_field_info));
    if (!fi)
    {
      fprintf(stderr,"dbfawk_field_list: first calloc failed\n");
      return NULL;
    }

    if (prev)
    {
      prev->next = fi;
    }
    else                        /* no prev, must be first one */
    {
      head = fi;
    }
    d = fi->name;
    while (*p && *p != ':')
    {
      *d++ = *p++;
    }
    if (*p == ':')
    {
      *p++ = '\0';
    }
    *d='\0';
    fi->num = DBFGetFieldIndex(dbf, fi->name);
    fi->type = DBFGetFieldInfo(dbf, fi->num, junk, &w, &prec);
    sp = p;
    prev = fi;
  }

  return head;
}





/*
 * dbfawk_load_sigs:  Load up dbfawk signature mappings
 *  Reads *.dbfawk and registers dbffields "signature".
 *  Returns head of sig_info list.
 *
 *  TODO - consider whether it makes sense to use a private symtbl,
 *   compile and then free here or require the caller to pass in a
 *   symtbl that has dbfinfo declared.
 */
// Malloc's dbfawk_sig_info and returns a filled-in list

dbfawk_sig_info *dbfawk_load_sigs(const char *dir, /* directory path */
                                  const char *ftype) /* filetype */
{
  DIR *d;
  struct dirent *e;
  struct stat nfile;
  char fullpath[MAX_FILENAME];
  int ftlen;
  dbfawk_sig_info *i = NULL, *head = NULL;
  awk_symtab *symtbl;
  char dbfinfo[1024];         /* local copy of signature */

  if (!dir || !ftype)
  {
    return NULL;
  }
  ftlen = strlen(ftype);
  d = opendir(dir);
  if (!d)
  {
    return NULL;
  }

  symtbl = awk_new_symtab();
  if (!symtbl)
  {
    return NULL;
  }

  awk_declare_sym(symtbl,"dbfinfo",STRING,dbfinfo,sizeof(dbfinfo));

  while ((e = readdir(d)) != NULL)
  {
    int len = strlen(e->d_name);
    char *path = calloc(1,len+strlen(dir)+2);

    // Check for hidden files or directories
    if (e->d_name[0] == '.')
    {
      // Hidden, skip it
      free(path);
      continue;
    }

    // Check for regular files

    xastir_snprintf(fullpath,
                    sizeof(fullpath),
                    "%s/%s",
                    dir,
                    e->d_name);

    if (stat(fullpath, &nfile) != 0)
    {
      // Couldn't stat file
      free(path);
      continue;
    }

    if ((nfile.st_mode & S_IFMT) != S_IFREG)
    {
      // Not a regular file
      free(path);
      continue;
    }

    if (!path)
    {
      if (symtbl)
      {
        awk_free_symtab(symtbl);
      }
      fprintf(stderr,"failed to malloc in dbfawk.c!\n");
      closedir(d);
      return NULL;
    }

    *dbfinfo = '\0';
    if (len > ftlen && (strcmp(&e->d_name[len-ftlen],ftype) == 0))
    {
      if (!head)
      {
        i = head = calloc(1,sizeof(dbfawk_sig_info));

        if (!i)
        {
          fprintf(stderr,"failed to malloc in dbfawk.c!\n");
          free(path);
          if (symtbl)
          {
            awk_free_symtab(symtbl);
          }
          closedir(d);
          return NULL;
        }
      }
      else
      {
        i->next = calloc(1,sizeof(dbfawk_sig_info));

        if (!i->next)
        {
          fprintf(stderr,"failed to malloc in dbfawk.c!\n");
          free(path);
          if (symtbl)
          {
            awk_free_symtab(symtbl);
          }
          closedir(d);
          return head; // Return what we were able to gather.
        }

        i = i->next;
      }

      xastir_snprintf(path,
                      len+strlen(dir)+2,
                      "%s/%s",
                      dir,
                      e->d_name);

      i->prog = awk_load_program_file(path);

      if (awk_compile_program(symtbl,i->prog) < 0)
      {
        fprintf(stderr,"%s: failed to parse\n",e->d_name);
      }
      else
      {
        /* dbfinfo must be defined in BEGIN rule */
        awk_exec_begin(i->prog);

        i->sig = strdup(dbfinfo);

        awk_uncompile_program(i->prog);
      }
    }
    free(path);
  }

  closedir(d);

  if (symtbl)
  {
    awk_free_symtab(symtbl);
  }

  return head;
}





void dbfawk_free_sig(dbfawk_sig_info *ptr)
{

  if (ptr)
  {
    if (ptr->prog)
    {
      awk_free_program(ptr->prog);
    }

    if (ptr->sig)
    {
      free(ptr->sig);
    }
    free(ptr);
  }
}





void dbfawk_free_sigs(dbfawk_sig_info *list)
{
  dbfawk_sig_info *x, *p;

  for (p = list; p; )
  {
    x = p;
    p = p->next;
    dbfawk_free_sig(x);
  }
}





/*
 * dbfawk_find_sig:  Given a DBF file's "signature", find the appropriate
 * awk program.  If filename is not null, see if there's a per-file .dbfawk
 * and load it.
 */

dbfawk_sig_info *dbfawk_find_sig(dbfawk_sig_info *Dbf_sigs,
                                 const char *sig,
                                 const char *file)
{
  dbfawk_sig_info *result = NULL;

  if (file)
  {
    int perfilesize=strlen(file)+8;
    char *dot, *perfile = calloc(1,perfilesize);
    dbfawk_sig_info *info;

    if (!perfile)
    {
      fprintf(stderr,"failed to malloc in dbfawk_find_sig!\n");
      return NULL;
    }

    xastir_snprintf(perfile,
                    perfilesize-1,
                    "%s",
                    file);

    dot = strrchr(perfile,'.');

    if (dot)
    {
      *dot = '\0';
    }

    strncat(perfile, ".dbfawk", perfilesize-1);

    info = calloc(1,sizeof(*info));

    if (!info)
    {
      fprintf(stderr,"failed to malloc in dbfawk_find_sig!\n");
      free(perfile);
      return NULL;
    }
    info->prog = awk_load_program_file(perfile);
    /* N.B. info->sig is left NULL since it won't be searched, and
       to flag that it's safe to free this memory when we're done with
       it */
    info->sig = NULL;
    free(perfile);
    if (info->prog)
    {

      return info;
    }
    else
    {
      dbfawk_free_sigs(info);
    }
    /* fall through and do normal signature search */
  }

  for (result = Dbf_sigs; result; result = result->next)
  {
    if (strcmp(result->sig,sig) == 0)
    {
      return result;
    }
  }
  return NULL;
}





/*
 * dbfawk_parse_record:  Read a dbf record and parse only the fields
 *  listed in 'fi' using the program, 'rs'.
 */
void dbfawk_parse_record(awk_program *rs,
                         DBFHandle dbf,
                         dbfawk_field_info *fi,
                         int i)
{
  dbfawk_field_info *finfo;

  awk_exec_begin_record(rs); /* execute a BEGIN_RECORD rule if any */

  for (finfo = fi; finfo ; finfo = finfo->next)
  {
    char qbuf[1024];

    switch (finfo->type)
    {
      case FTString:
        sprintf(qbuf,"%s=%s",finfo->name,DBFReadStringAttribute(dbf,i,finfo->num));
        break;
      case FTInteger:
        sprintf(qbuf,"%s=%d",finfo->name,DBFReadIntegerAttribute(dbf,i,finfo->num));
        break;
      case FTDouble:
        sprintf(qbuf,"%s=%f",finfo->name,DBFReadDoubleAttribute(dbf,i,finfo->num));
        break;
      case FTInvalid:
      default:
        sprintf(qbuf,"%s=??",finfo->name);
        break;
    }
    if (awk_exec_program(rs,qbuf,strlen(qbuf)) == 2)
    {
      break;
    }
  }
  awk_exec_end_record(rs); /* execute an END_RECORD rule if any */
}





#ifndef HAVE_DBFGETFIELDINDEX
#include <ctype.h>
/************************************************************************/
/*                            str_to_upper()                            */
/************************************************************************/

static void str_to_upper (char *string)
{
  int len;
  short i = -1;

  len = strlen (string);

  while (++i < len)
    if (isalpha(string[i]) && islower(string[i]))
    {
      string[i] = toupper ((int)string[i]);
    }
}





/************************************************************************/
/*                          DBFGetFieldIndex()                          */
/*                                                                      */
/*      Get the index number for a field in a .dbf file.                */
/*                                                                      */
/*      Contributed by Jim Matthews.                                    */
/************************************************************************/

int DBFGetFieldIndex(DBFHandle psDBF, const char *pszFieldName)
{
  char          name[12], name1[12], name2[12];
  int           i;

  xastir_snprintf(name1,
                  sizeof(name1),
                  "%s",
                  pszFieldName);

  str_to_upper(name1);

  for( i = 0; i < DBFGetFieldCount(psDBF); i++ )
  {
    DBFGetFieldInfo( psDBF, i, name, NULL, NULL );
    xastir_snprintf(name2,
                    sizeof(name2),
                    "%s",
                    name);
    str_to_upper(name2);

    if(!strncmp(name1,name2,10))
    {
      return(i);
    }
  }
  return(-1);
}



#endif
#endif /* HAVE_LIBSHP && HAVE_LIBPCRE */


