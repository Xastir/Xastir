/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2004  The Xastir Group
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */


#ifndef XASTIR_MAP_CACHE_H
#define XASTIR_MAP_CACHE_H


extern int map_cache_put( char * map_cache_url, char * map_cache_file );
extern int map_cache_get( char * map_cache_url, char * map_cache_file );
extern int map_cache_del( char * map_cache_url );
extern int map_cache_expired( char * mc_filename, time_t mc_max_age );
extern char * map_cache_fileid();

// about 6mo
#define MC_MAX_FILE_AGE 6*30*24*60*60

// 1 hr
//#define MC_MAX_FILE_AGE 60*60

// 5 seconds -- don't do this except for testing 
//#define MC_MAX_FILE_AGE 5


#endif /* XASTIR_MAP_CACHE_H */


