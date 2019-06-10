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


#ifndef __XASTIR_IGATE_H
#define __XASTIR_IGATE_H

typedef struct
{
  char call[12];
} NWS_Data;

extern void igate_init(void);
extern void insert_into_heard_queue(int port, char *line);
extern void output_igate_net(char *line, int port, int third_party);
extern void output_igate_rf(char *from, char *call, char *path, char *line, int port, int third_party, char *object_name);
extern void output_nws_igate_rf(char *from, char *path, char *line, int port, int third_party);

#endif  // __XASTIR_IGATE_H


