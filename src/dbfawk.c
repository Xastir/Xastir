/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2003  The Xastir Group
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
#include "config.h"
#if defined(HAVE_LIBSHP) && defined(HAVE_LIBPCRE)
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include "awk.h"
#include "dbfawk.h"

/*
 * dbfawk_sig:  Generate a signature for a DBF file.
 *  Fills in sig and returns number of fields.
 */
int dbfawk_sig(DBFHandle dbf, char *sig, int size)
{
  int nf = 0;

  if (sig && size > 0 && dbf) {
    int i;
    char *sp;
    int width,prec;
    
    nf = DBFGetFieldCount(dbf);
    for (i = 0, sp=sig; sp < &sig[size-XBASE_FLDHDR_SZ] && i < nf ; i++) {
      DBFGetFieldInfo(dbf,i,sp,&width,&prec);
      sp += strlen(sp);
      *sp++ = ':';              /* field name separator */
    }
    if (i)
      *--sp = '\0';             /* clobber the trailing sep */
  }
  return nf;
}

/*
 * dbfawk_field_list: Generate a list of info about fields to read for
 *  a given a DBFHandle and colon-separated list of fieldnames.
 */
dbfawk_field_info *dbfawk_field_list(DBFHandle dbf, char *dbffields)
{
  dbfawk_field_info *fi, *head = NULL, *prev;
  int nf;
  char *sp;

  /* now build up the list of fields to read */
  for (nf = 0, sp = dbffields, prev = NULL; *sp; nf++) {
    char *d,*p = sp;
    char junk[XBASE_FLDHDR_SZ];
    int w,prec;
    
    fi = calloc(1,sizeof(dbfawk_field_info));
    if (!fi)
        return NULL;
    if (prev) {
        prev->next = fi;
    } else {                    /* no prev, must be first one */
        head = fi;
    }
    d = fi->name;
    while (*p && *p != ':') *d++ = *p++;
    if (*p == ':')
      *d = *p++ = '\0';
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

dbfawk_sig_info *dbfawk_load_sigs(const char *dir, /* directory path */
                                  const char *ftype) /* filetype */
{
    DIR *d;
    struct dirent *e;
    int ftlen;
    dbfawk_sig_info *i = NULL, *head = NULL;
    awk_symtab *symtbl;
    char dbfinfo[1024];         /* local copy of signature */

    if (!dir || !ftype)
        return NULL;
    ftlen = strlen(ftype);
    d = opendir(dir);
    if (!d)
        return NULL;

    symtbl = awk_new_symtab();
    awk_declare_sym(symtbl,"dbfinfo",STRING,dbfinfo,sizeof(dbfinfo));

    while ((e = readdir(d)) != NULL) {
        int len = strlen(e->d_name);
        char *path = calloc(1,len+strlen(dir)+2);

        if (!path) {
            fprintf(stderr,"failed to malloc in dbfawk.c!\n");
            return NULL;
        }
        *dbfinfo = '\0';
        if (len > ftlen && (strcmp(&e->d_name[len-ftlen],ftype) == 0)) {
            if (!head) {
                i = head = calloc(1,sizeof(dbfawk_sig_info));
            } else {
                i->next = calloc(1,sizeof(dbfawk_sig_info));
                i = i->next;
            }
            strcpy(path,dir);
            strcat(path,"/");
            strcat(path,e->d_name);
            i->prog = awk_load_program_file(path);
            free(path);
            if (awk_compile_program(symtbl,i->prog) < 0) {
                fprintf(stderr,"%s: failed to parse\n",e->d_name);
            } else {
                /* dbfinfo must be defined in BEGIN rule */
                awk_exec_begin(i->prog); 
                i->sig = strdup(dbfinfo);
                awk_uncompile_program(i->prog);
            }
        }
    }

    awk_free_symtab(symtbl);
    return head;
}

void dbfawk_free_sig(dbfawk_sig_info *sig) 
{
    if (sig) {
        if (sig->prog)
            awk_free_program(sig->prog);
        if (sig)
            free(sig);
    }
}

void dbfawk_free_sigs(dbfawk_sig_info *list) 
{
    dbfawk_sig_info *x, *p;

    for (p = list; p; ) {
        x = p;
        p = p->next;
        dbfawk_free_sig(x);
    }
}

/*
 * dbfawk_find_sig:  Given a DBF file's "signature", find the appropriate
 * awk program.  
 */

dbfawk_sig_info *dbfawk_find_sig(dbfawk_sig_info *info, const char *sig)
{
    dbfawk_sig_info *result = NULL;

    for (result = info; result; result = result->next) {
        if (strcmp(result->sig,sig) == 0)
            return result;
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

    for (finfo = fi; finfo ; finfo = finfo->next) {
        char qbuf[1024];

        switch (finfo->type) {
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
        awk_exec_program(rs,qbuf,strlen(qbuf));
    }
    awk_exec_end_record(rs); /* execute an END_RECORD rule if any */
}

#endif /* HAVE_LIBSHP && HAVE_LIBPCRE */
