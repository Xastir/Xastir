/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000,2001,2002  The Xastir Group
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


#define MAX_VALUE 300

extern time_t next_time;

void store_string(FILE *fout,char *option, char *value);
void store_char(FILE *fout,char *option, char value);
void store_int(FILE *fout,char *option, int value);
void store_long(FILE *fout,char *option, long value);
int get_string(char *option, char *value);
int get_char(char *option, char *value);
int get_int(char *option, int *value);
int get_long(char *option, long *value);
char *get_user_base_dir(char *dir);
char *get_data_base_dir(char *dir);
void save_data(void);
void load_data_or_default(void);

