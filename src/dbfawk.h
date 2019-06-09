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
#ifndef DBFAWK_H
#define DBFAWK_H

#ifndef _SHAPEFILE_H_INCLUDED
  #ifdef HAVE_SHAPEFIL_H
    #include <shapefil.h>
  #else
    #ifdef HAVE_LIBSHP_SHAPEFIL_H
      #include <libshp/shapefil.h>
    #else
      #error HAVE_LIBSHP defined but no corresponding include defined
    #endif // HAVE_LIBSHP_SHAPEFIL_H
  #endif // HAVE_SHAPEFIL_H
#endif // _SHAPEFILE_H_INCLUDED

typedef struct dbfawk_field_info_
{
  struct dbfawk_field_info_ *next;
  char name[XBASE_FLDHDR_SZ];   /* name of the field */
  int num;                      /* column number */
  DBFFieldType type;            /* data type */
} dbfawk_field_info;

typedef struct dbfawk_sig_info_
{
  struct dbfawk_sig_info_ *next;
  char *sig;                  /* dbfinfo signature */
  awk_program *prog;          /* the program for this signature */
} dbfawk_sig_info;

extern int dbfawk_sig(DBFHandle dbf, char *sig, int size);
extern dbfawk_field_info *dbfawk_field_list(DBFHandle dbf, char *dbffields);
extern dbfawk_sig_info *dbfawk_load_sigs(const char *dir, const char *ftype);
extern dbfawk_sig_info *dbfawk_find_sig(dbfawk_sig_info *info,
                                        const char *sig,
                                        const char *file);
extern void dbfawk_free_sig(dbfawk_sig_info *sig);
extern void dbfawk_free_sigs(dbfawk_sig_info *list);
extern void dbfawk_free_info(dbfawk_field_info *list);
extern void dbfawk_parse_record(awk_program *rs,
                                DBFHandle dbf,
                                dbfawk_field_info *fi,
                                int i);
#endif /* !DBFAWK_H*/
