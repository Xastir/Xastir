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
 * FCC Database structures
 *
 */

/*
type_purpose - Indicates the reason why the application was
               filed.  Multiple codes may occur.  Codes are:

    A  New license
    B  Change existing class
    C  Change name
    D  Change mailing address
    E  Change callsign
    F  Renewal on Form 610
    G  Add record (internal)
    H  Duplicate license request
    I  Change Issue/Expiration Date
    J  Supercede
    K  Internal correction code
    L  Delete
    N  Renewal on form 610R
    O  Renewal on form 610B
    P  Modification on form 610B
    Q  Restore both database and pending
    R  Restore database
    S  Special callsign change

type_applicant - Indicates type of application.  Codes are:

    A  Alien
    C  Club
    I  Individual
    M  Military recreation
    R  RACES

*/

#ifndef XASTIR_FCC_DATA_H
#define XASTIR_FCC_DATA_H

#define FCC_DATA_LEN 200

typedef struct
{
  char id_callsign[11];
  char id_file_num[11];
  char type_purpose[9];
  char type_applicant;
  char name_licensee[41];
  char text_street[36];
  char text_pobox[21];
  char city[30];
  char state[3];
  char zipcode[10];
  char filler;
  char date_issue[7];
  char date_expire[7];
  char date_last_change[7];
  char id_examiner[4];
  char renewal_notice;
} FccAppl;

extern int check_fcc_data(void);
extern int search_fcc_data_appl(char *callsign, FccAppl *data);

#endif /* XASTIR_FCC_DATA_H */
