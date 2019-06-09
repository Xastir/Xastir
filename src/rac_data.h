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

/*
 * Industry Canada/RAC  Database structure
 * 472 bytes -- filler is club info
 * qual fields represent basic, 5wpm, 12wpm, advanced
 */

#ifndef __XASTIR_RAC_DATA_H
#define __XASTIR_RAC_DATA_H

#define RAC_DATA_LEN 472

typedef struct
{
  char callsign[7];
  char first_name[36];
  char last_name[36];
  char address[71];
  char city[36];
  char province[3];
  char postal_code[11];
  char qual_a[2];
  char qual_b[2];
  char qual_c[2];
  char qual_d[2];
  char club_name[142];
  char club_address[71];
  char club_city[36];
  char club_province[3];
  char club_postal_code[10];
  char crlf[2];
  char filler[8]; // To prevent overruns
} rac_record;

extern int search_rac_data(char *callsign, rac_record *data);
extern int search_rac_data_appl(char *callsign, rac_record *data);
extern int check_rac_data(void);


#endif /* __XASTIR_RAC_DATA_H */

