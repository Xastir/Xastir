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
#ifdef HAVE_LIBSHP
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "awk.h"
#include "dbfawk.h"

static awk_symtab *symtbl = NULL;

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
      *sp++ = ':';		/* field name separator */
    }
    if (i)
      *--sp = '\0';		/* clobber the trailing sep */
  }
  return nf;
}

/*
 * dbfawk_field_list: Generate a list of info about fields to read for
 *  a given a DBFHandle and colon-separated list of fieldnames.
 */
dbfawk_field_info *dbfawk_field_list(DBFHandle dbf, char *dbffields)
{
  dbfawk_field_info *fi, *head, *prev;
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
 * dbfawk_load_sig:  Load up dbfawk signature mappings
 *  Reads config/*.dbfawk and registers dbffields "signature".
 */

/*
 * dbfawk_find_sig:  Given a DBF file's "signature", find (and
 *  compile if necessary) the appropriate dbfawk program.
 */

#endif /* HAVE_LIBSHP */
