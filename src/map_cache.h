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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */


#ifndef XASTIR_MAP_CACHE_H
#define XASTIR_MAP_CACHE_H

// Global variable declarations
extern int map_cache_fetch_disable;


// External function declarations

extern void map_cache_init(void);

// Saves file and puts entries into cache db
extern int map_cache_put( char * map_cache_url, char * map_cache_file );

// Retrieves entry from cache db - checks existance of file
extern int map_cache_get( char * map_cache_url, char * map_cache_file );

// Deletes cached map file and the entry from cache
extern int map_cache_del( char * map_cache_url );

// Checks to see if map is expired based on date embedded in filename
extern int map_cache_expired( char * mc_filename, time_t mc_max_age );

// Generates filename based on current time
extern char * map_cache_fileid(void);


// Static variable definitions
// These should probably be runtime options

// Cache expiration times
// about 6mo
#define MC_MAX_FILE_AGE 6*30*24*60*60

// 1 hr
//#define MC_MAX_FILE_AGE 60*60

// 5 seconds -- don't do this except for testing
//#define MC_MAX_FILE_AGE 5

// Cache Space Limit in bytes

// 1 megabytes == about ten 1024x768 map gifs n8ysz
// MAP_CACHE_SPACE_LIMIT=1024*1024

// 16 megabytes
// MAP_CACHE_SPACE_LIMIT=16*1024*1024

// 128 megabytes
#define MAP_CACHE_SPACE_LIMIT 128*1024*1024


#endif /* XASTIR_MAP_CACHE_H */
