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


/*
 *   The code in this file is used to cache maps downloaded
 *   from tiger.census.gov, and to manage that cache. It was written
 *   to use Berkeley DB version 4 or better.
 *
 *   Dan Brown N8YSZ.
 *
 */


// Need this one before the #ifdef in order to get the definition of
// USE_MAP_CACHE, if defined.
#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H


#ifdef  USE_MAP_CACHE
//#warning USE_MAP_CACHE Defined

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "snprintf.h"
#include "xastir.h"
#include "xa_config.h"
#include "main.h"
#include "maps.h"
#include "map_cache.h"
#include <db.h>

// Must be last include file
#include "leak_detection.h"





// This is used to temporarily disable fetching from the map cache.
// Used for refreshing corrupted maps in the cache.
int map_cache_fetch_disable = 0;


// Used to disable map caching entirely if the header and dblib
// versions don't match, in order to avoid segfaults when the map
// caching is used.
int map_cache_disabled = 0;





// Here we do a run-time check to verify that the header file we
// used to compile with is the same version as the shared library
// we're currently linked with.  To do otherwise often results in
// segfaults.
//
void map_cache_init(void)
{
  int warn_now = 0;

  if (strcmp( DB_VERSION_STRING, db_version(NULL,NULL,NULL) ) != 0)
  {
    fprintf(stderr,
            "\n\n***** WARNING *****\n");
    fprintf(stderr,
            "Berkeley DB header files/shared library file do NOT match!\n");
    fprintf(stderr,
            "Disabling use of the map cache.\n");


// Can't bring up a popup here 'cuz we don't have any GUI running
// yet by this point.
// popup_message_always(langcode("POPEM00034"),langcode("POPEM00046"));


    warn_now++;
    map_cache_disabled++;
  }

  if (debug_level & 5 || warn_now)
  {

    //fprintf(stderr,
    //    "Berkeley DB Library Header File Version %d.%d.%d\n",
    //    DB_VERSION_MAJOR,
    //    DB_VERSION_MINOR,
    //    DB_VERSION_PATCH);

    fprintf(stderr,
            " Header file: %s\n",
            DB_VERSION_STRING);

    fprintf(stderr,
            "Library file: %s\n",
            db_version(NULL,NULL,NULL) );
  }

  if (warn_now)
  {
    fprintf(stderr,
            "***** WARNING *****\n");
  }
}





// map_cache_put()
//
// Inputs:
//
// Outputs:
//
int map_cache_put( char * map_cache_url, char * map_cache_file )
{

// Puts an entry into the url->filename database
// Tracks space used in "CACHE_SPACE_USED"

  char mc_database_filename[MAX_FILENAME];
  int mc_ret, mc_t_ret, mc_file_stat, mc_space_used ;
  DBT mc_key, mc_data ;
  DB *dbp;
  struct stat file_status;
  char mc_buf[128];
  char temp_file_path[MAX_VALUE];

  if (map_cache_disabled)
  {
    return(1);
  }

  mc_space_used=0;

  xastir_snprintf(mc_database_filename,
                  sizeof(mc_database_filename),
                  "%s/map_cache.db",
                  get_user_base_dir("map_cache", temp_file_path, sizeof(temp_file_path)));

  // check for reasonable filename
  // expects file name like /home/brown/.xastir/map_cache/map_1100052372.gif
  //                                   1234567890123456789012345678901234567

  mc_ret=strlen(map_cache_file);

  if ( mc_ret < 37 )
  {
    if (debug_level & 512 )
    {
      fprintf(stderr,
              "map_cache_put: Unusable filename: %s. Skipping encaching\n",
              (map_cache_file == NULL) ? "(null)" : map_cache_file);
    }
    return (-1 * mc_ret);
  }

// stat the file to see if we even need to bother

  if ((mc_file_stat=stat(map_cache_file, &file_status)) !=0)
  {
    if (debug_level & 512 )
    {
      fprintf(stderr,
              "map_cache_put: File Error: %s. Skipping encaching\n",
              (map_cache_file == NULL) ? "(null)" : map_cache_file);
    }

    return (mc_file_stat);
  }

  if ( debug_level & 512)
  {
    fprintf (stderr, "map_cache_put: file_status.st_size %d\n",
             (int) file_status.st_size);
  }

// Create handle to db

  if ((mc_ret = db_create(&dbp, NULL, 0)) != 0)
  {
    fprintf(stderr, "map_cache_put db_create:%s\n", db_strerror(mc_ret));
    return(1);
  }

// open the db

#if  (DB_VERSION_MAJOR<4)   /** DB_VERSION Check **/
#error DB_VERSION_MAJOR < 4

#elif (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR<=0 )

  if ((mc_ret = dbp->open(dbp,
                          mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0)
  {

    dbp->err(dbp, mc_ret, "%s", mc_database_filename);
    db_strerror(mc_ret);
  }
#else

  if ((mc_ret = dbp->open(dbp,
                          NULL,mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0)
  {

    dbp->err(dbp, mc_ret, "%s", mc_database_filename);
    db_strerror(mc_ret);
  }

#endif  /** DB_VERSION Check **/

// Before we put something in we need to see if we got space

// if mc_cache_size_limit

// Setup for get

  memset(&mc_key, 0, sizeof(mc_key));
  memset(&mc_data, 0, sizeof(mc_data));

  mc_key.data = "CACHE_SPACE_USED";
  mc_key.size = sizeof("CACHE_SPACE_USED");

// check "CACHE_SPACE_USED" record in db

// mc_ret is assigned here but not used.  Commented it out for now.
  if (((/* mc_ret = */ dbp->get(dbp, NULL, &mc_key, &mc_data, 0)) == 0) &&  ( mc_data.data != NULL ) )
  {

    if ( debug_level & 512 )
    {
      fprintf(stderr, "map_cache_put: %s: key retrieved: data was %s.\n",
              (mc_key.data == NULL) ? "(null)" : (char *)mc_key.data,
              (mc_data.data == NULL) ? "(null)" : (char *)mc_data.data);
    }

    if (mc_data.data == NULL)
    {
      mc_space_used = 0;
    }
    else
    {
      mc_space_used = atoi( (char *)mc_data.data);
    }

    if ( debug_level & 512 )
    {
      fprintf (stderr, "map_cache_put: CACHE_SPACE_USED = %.2f mb\n",
               (mc_space_used/1024/1024.0));
    }

  }
  else
  {

    if (mc_data.data == NULL)
    {
      if ( debug_level & 512 )
      {
        fprintf (stderr, "map_cache_put: CACHE_SPACE_USED get returned null \n");
      }
    }
    else
    {
      if ( debug_level & 512 )
      {
        fprintf (stderr, "map_cache_put: Unable to check CACHE_SPACE_USED: %s\n",
                 db_strerror(mc_ret));
      }
    }

    // for now let us assume this is the first map entry and  we
    // just flag an error. Better procedure for later might be to
    // return(mc_ret) indicating a database error of some sort

  }

//      xastir_snprintf(map_cache_file, MAX_FILENAME, "%s",(char *)mc_data.data);

  if ( debug_level & 512 )
  {
    fprintf (stderr, "map_cache_put: mc_space_used before = %d bytes file_status.st_size %d\n",
             mc_space_used,
             (int) file_status.st_size);
  }

  mc_space_used += (int) file_status.st_size;

  if ( debug_level & 512 )
  {
    fprintf (stderr, "map_cache_put: mc_space_used after = %d bytes \n",
             mc_space_used);
  }

  if ( mc_space_used > MAP_CACHE_SPACE_LIMIT)
  {
    if ( debug_level & 512 )
    {
      fprintf (stderr, "map_cache_put: Warning cache space used: %.2f mb NOW OVER LIMIT: %.2f mb\n",
               (mc_space_used/1024/1024.0),
               (MAP_CACHE_SPACE_LIMIT/1024/1024.0));
    }

// Cache Cleanup
// The warning is nice, but we should do something here
// Needs LRU and or FIFO db structures

  }
  else
  {

// else put cache_space_used

// setup

    memset(&mc_key, 0, sizeof(mc_key));
    memset(&mc_data, 0, sizeof(mc_data));

// data

    mc_key.data = "CACHE_SPACE_USED";
    mc_key.size = sizeof("CACHE_SPACE_USED");

    xastir_snprintf(mc_buf, sizeof(mc_buf), "%d", mc_space_used);

    if ( debug_level & 512 )
    {
      fprintf (stderr, "map_cache_put: mc_buf: %s len %d\n",
               mc_buf,(int) sizeof(mc_buf));
    }

    mc_data.data = mc_buf ;
    mc_data.size = sizeof(mc_buf);

// put

    if ((mc_ret = dbp->put(dbp, NULL, &mc_key, &mc_data, 0)) == 0)
    {

      if ( debug_level & 512 )
      {
        fprintf(stderr, "map_cache_put: %s: key stored.\n",
                (mc_key.data == NULL) ? "(null)" : (char *)mc_key.data);
      }

    }
    else
    {

      if ( debug_level & 512 )
      {
        dbp->err(dbp, mc_ret, "DB->put");
      }
//            db_strerror(mc_ret);

      return(mc_ret);

    }

  }

// Setup for put of data

  memset(&mc_key, 0, sizeof(mc_key));
  memset(&mc_data, 0, sizeof(mc_data));

// Real data at last

  mc_key.data = map_cache_url;
  mc_key.size = strlen(map_cache_url);
  mc_data.data = map_cache_file;
  mc_data.size = strlen(map_cache_file)+1; /* +1 includes  \0 */

// do the actual put

  if ((mc_ret = dbp->put(dbp, NULL, &mc_key, &mc_data, 0)) == 0)
  {
    if ( debug_level & 512 )
    {
      fprintf(stderr, "map_cache_put: %s: key stored.\n",
              (mc_key.data == NULL) ? "(null)" : (char *)mc_key.data);
    }
  }
  else
  {
    if ( debug_level & 512 )
    {
      dbp->err(dbp, mc_ret, "DB->put") ;
    }
    // db_strerror(mc_ret);
    return(mc_ret);
  }

  // Map now cached
  statusline(langcode("CACHE001"), 1);

// close the db

  // Only try the close if we have a valid handle
  if (dbp != NULL)
  {
    if ((mc_t_ret = dbp->close(dbp, 0)) != 0 && mc_ret == 0)
    {
      mc_ret = mc_t_ret;
      db_strerror(mc_ret);
    }
  }

  /* end map_cache_put */

  return (0) ;
}





// map_cache_get()
//
// Queries URL->Filename db
// Calls map_cache_del to cleanup expired maps
//
// Inputs:
//
// Outputs: 0 if cached file is retrieved successfully
//          1 if db can't be created
//          negative number if unusable filename or bad filestat
//          positive number if map has expired
//          return value from dbp->get if record not found
//
int map_cache_get( char * map_cache_url, char * map_cache_file )
{
  DBT mc_key, mc_data ;
  DB *dbp;
  int mc_ret, mc_t_ret, mc_file_stat ;
  char mc_database_filename[MAX_FILENAME];
  struct stat file_status;
  char temp_file_path[MAX_VALUE];

  if (map_cache_disabled)
  {
    return(1);
  }

  set_dangerous("map_cache_get: xastir_snprintf 1");
  xastir_snprintf(mc_database_filename,
                  sizeof(mc_database_filename),   // change to max_filename?
                  "%s/map_cache.db",
                  get_user_base_dir("map_cache", temp_file_path, sizeof(temp_file_path)));
  clear_dangerous();

  set_dangerous("map_cache_get: db_create");
  if ((mc_ret = db_create(&dbp, NULL, 0)) != 0)
  {
    fprintf(stderr, "map_cache_get db_create:%s\n", db_strerror(mc_ret));
    return(1);
  }
  clear_dangerous();

#if  (DB_VERSION_MAJOR<4)   /** DB_VERSION Check **/
#error DB_VERSION_MAJOR < 4

#elif (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR<=0 )

  set_dangerous("map_cache_get:dbp->open 1");
  if ((mc_ret = dbp->open(dbp,
                          mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0)
  {
    ( debug_level & 512 ) ?  dbp->err(dbp, mc_ret, "%s", mc_database_filename):0;
    // db_strerror(mc_ret);
  }
  clear_dangerous();

#else

  set_dangerous("map_cache_get:dbp->open 2");
  if ((mc_ret = dbp->open(dbp,
                          NULL,mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0)
  {
    if (debug_level & 512)
    {
      dbp->err(dbp, mc_ret, "%s", mc_database_filename);
      // db_strerror(mc_ret);
    }
  }
  clear_dangerous();

#endif  /** DB_VERSION Check **/

  memset(&mc_key, 0, sizeof(mc_key));
  memset(&mc_data, 0, sizeof(mc_data));

  mc_key.data=map_cache_url ;
  mc_key.size=strlen(map_cache_url);

  statusline("Checking Map Cache",1);
  if (debug_level & 512 )
  {
    fprintf(stderr, "map_cache_get: Checking Map Cache\n");
  }


  set_dangerous("map_cache_get:dbp->get");
  if ((mc_ret = dbp->get(dbp, NULL, &mc_key, &mc_data, 0)) == 0)
  {
    if ( debug_level & 512 )
    {
      fprintf(stderr, "map_cache_get: %s: key retrieved: data was %s.\n",
              (mc_key.data == NULL) ? "(null)" : (char *)mc_key.data,
              (mc_data.data == NULL) ? "(null)" : (char *)mc_data.data);
    }
    set_dangerous("map_cache_get: xastir_snprintf 2");
    xastir_snprintf(map_cache_file, MAX_FILENAME, "%s",(char *)mc_data.data);
    clear_dangerous();

    // check for reasonable filename
    // expects file name like /home/brown/.xastir/map_cache/map_1100052372.gif
    //                                   1234567890123456789012345678901234567

    mc_ret=strlen(map_cache_file);

    if ( mc_ret < 37 )
    {

      if (debug_level & 512 )
      {
        fprintf(stderr,
                "map_cache_get: Unusable filename: %s. Deleting key %s from cache\n",
                (map_cache_file == NULL) ? "(null)" : map_cache_file,
                (map_cache_url == NULL) ? "(null)" : map_cache_url);
      }
      set_dangerous("map_cache_get: map_cache_del 1");
      map_cache_del(map_cache_url);
      if (dbp != NULL)
      {
        if ((mc_t_ret = dbp->close(dbp, 0)) != 0 && mc_ret == 0)
        {
          mc_ret = mc_t_ret;
          // db_strerror(mc_ret);
        }
      }
      clear_dangerous();

      return (-1 * mc_ret);
    }


    // check age of file - based on name - delete if too old

    if (debug_level & 512 )
    {
      fprintf(stderr, "map_cache_get: Checking age\n");
    }

    set_dangerous("map_cache_get: map_cache_expired");
    if ( (mc_ret=map_cache_expired(map_cache_file, (MC_MAX_FILE_AGE))))
    {
      if ( debug_level & 512 )
      {
        fprintf(stderr, "map_cache_get: deleting expired key: %s.\n",
                (mc_key.data == NULL) ? "(null)" : (char *)mc_key.data);
      }
      set_dangerous("map_cache_get: map_cache_del 2");
      map_cache_del(map_cache_url);
      if (dbp != NULL)
      {
        if ((mc_t_ret = dbp->close(dbp, 0)) != 0 && mc_ret == 0)
        {
          mc_ret = mc_t_ret;
          // db_strerror(mc_ret);
        }
      }
      clear_dangerous();
      return (mc_ret);
    }
    clear_dangerous();

    // check if the file exists
    if ( debug_level & 512 )
    {
      fprintf(stderr,"map_cache_get: checking for existence of map_cache_file:  %s.\n",
              (map_cache_file == NULL) ? "(null)" : map_cache_file);
    }

    mc_file_stat=stat(map_cache_file, &file_status);

    if ( debug_level & 512 )
    {
      fprintf(stderr,"map_cache_get: map_cache_file %s stat returned:%d.\n",
              (map_cache_file == NULL) ? "(null)" : map_cache_file,
              mc_file_stat);
    }

    if ( mc_file_stat == -1 )
    {
      //
      if ( debug_level & 512 )
      {
        fprintf(stderr,"map_cache_get: attempting to delete map_cache_file %s \n",
                (map_cache_file == NULL) ? "(null)" : map_cache_file);
      }

      set_dangerous("map_cache_get: dbp->del");
      if ((mc_ret = dbp->del(dbp, NULL, &mc_key, 0)) == 0)
      {
        if ( debug_level & 512 )
        {
          fprintf(stderr, "map_cache_get: File stat failed %s: key was deleted.\n",
                  (mc_key.data == NULL) ? "(null)" : (char *)mc_key.data);
        }
      }
      else
      {
        if ( debug_level & 512 )
        {
          dbp->err(dbp, mc_ret, "DB->del");
        }
        // db_strerror(mc_ret);
      }
      clear_dangerous();

      set_dangerous("map_cache_get: dbp->close 1");
      // Only try the close if we have a valid handle
      if (dbp != NULL)
      {
        if ((mc_t_ret = dbp->close(dbp, 0)) != 0 && mc_ret == 0)
        {
          mc_ret = mc_t_ret;
          // db_strerror(mc_ret);
        }
      }
      clear_dangerous();

      // db_strerror(mc_ret);
      // Return the file stat if there was a file problem
      return (mc_file_stat);
    }
    else
    {

      set_dangerous("map_cache_get: dbp->close 2");
      // Only try the close if we have a valid handle
      if (dbp != NULL)
      {
        if ((mc_t_ret = dbp->close(dbp, 0)) != 0 && mc_ret == 0)
        {
          mc_ret = mc_t_ret;
          // db_strerror(mc_ret);
        }
      }
      clear_dangerous();
      // If we made it here all is good

      // Loading Cached Map
      statusline(langcode("CACHE002"), 1);
      return (0);
    }
  }
  else
  {
    if ( debug_level & 512 )
    {
      fprintf(stderr, "map_cache_get: Get failed. Key was: %s.\n",
              (mc_key.data == NULL) ? "(null)" : (char *)mc_key.data);
    }

    if (debug_level & 512)
    {
      dbp->err(dbp, mc_ret, "DB->get");
      // db_strerror(mc_ret);
    }

    // there was some problem getting things from the db
    // return the return from the get

    // Map not found in cache...
    statusline(langcode("CACHE003"), 1);
    set_dangerous("map_cache_get: dbp->close 3");
    // Only try the close if we have a valid handle
    if (dbp != NULL)
    {
      if ((mc_t_ret = dbp->close(dbp, 0)) != 0 && mc_ret == 0)
      {
        mc_ret = mc_t_ret;
        // db_strerror(mc_ret);
      }
    }
    clear_dangerous();
    return (mc_ret);
  }

  clear_dangerous();

  /** end map_cache_get **/
}





// map_cache_del()
//
// Delete entry from the cache and unlink associated file from disk
//
// Inputs:  Map URL
//
// Outputs: 0 if successful deleting the item from the cache
//          1 if error in creating/opening DB file
//          mc_ret if unlink failed or if error in DB->del
//
int map_cache_del( char * map_cache_url )
{
  DBT mc_key, mc_data, mc_size_key, mc_size_data ;
  DB *dbp;
  int mc_ret, mc_t_ret, mc_file_stat, mc_space_used;
  char mc_database_filename[MAX_FILENAME];
  char mc_delete_file[MAX_FILENAME];
  struct stat file_status;
  char mc_buf[128];
  char temp_file_path[MAX_VALUE];

  if (map_cache_disabled)
  {
    return(1);
  }

  mc_space_used = 0 ;

  xastir_snprintf(mc_database_filename,
                  MAX_FILENAME,
                  "%s/map_cache.db",
                  get_user_base_dir("map_cache", temp_file_path, sizeof(temp_file_path)));

  if ((mc_ret = db_create(&dbp, NULL, 0)) != 0)
  {
    fprintf(stderr, "map_cache_del db_create:%s\n", db_strerror(mc_ret));
    return(1);
  }

#if  (DB_VERSION_MAJOR<4)   /** DB_VERSION Check **/
#error DB_VERSION_MAJOR < 4

#elif (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR<=0 )

  if ((mc_ret = dbp->open(dbp, mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0)
  {
    ( debug_level & 512 ) ?  dbp->err(dbp, mc_ret, "%s", mc_database_filename):0;
    // db_strerror(mc_ret);
    return(1);
  }
#else

  if ((mc_ret = dbp->open(dbp,
                          NULL,mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0)
  {
    if (debug_level & 512)
    {
      dbp->err(dbp, mc_ret, "%s", mc_database_filename);
      // db_strerror(mc_ret);
    }
    return(1);
  }

#endif  /** DB_VERSION Check **/

  memset(&mc_key, 0, sizeof(mc_key));
  memset(&mc_data, 0, sizeof(mc_data));

  mc_key.data=map_cache_url ;
  mc_key.size=strlen(map_cache_url);

  // Try to get the key from the cache
  if ((mc_ret = dbp->get(dbp, NULL, &mc_key, &mc_data, 0)) != 0)
  {
    // Couldn't get the key from the cache

    if ( debug_level & 512 )
    {
      dbp->err(dbp, mc_ret, "DB->del");
    }
    // db_strerror(mc_ret);

    // Only try the close if we have a valid handle
    if (dbp != NULL)
    {
      set_dangerous("map_cache_del: dbp->close 1");
      dbp->close(dbp, 0);
      clear_dangerous();
    }
    return (mc_ret);
  }

  if ( debug_level & 512 )
  {
    fprintf(stderr, "map_cache_del: %s: key retrieved: data was %s.\n",
            (mc_key.data == NULL) ? "(null)" : (char *)mc_key.data,
            (mc_data.data == NULL) ? "(null)" : (char *)mc_data.data);
  }

  // stat the file

  xastir_snprintf(mc_delete_file,
                  MAX_FILENAME,
                  "%s",
                  (char *) mc_data.data);

  set_dangerous("map_cache_del: stat");
  mc_file_stat = stat(mc_delete_file, &file_status);
  clear_dangerous();

  if ( debug_level & 512 )
  {
    fprintf(stderr,"map_cache_del: file %s stat returned:%d.\n",
            (mc_data.data == NULL) ? "(null)" : (char *) mc_data.data,
            mc_file_stat);
  }

  if (mc_file_stat != 0 )
  {

// file stat was not good - do something here
// RETURN() HERE?

  }


// Setup for get CACHE_SPACE_USED

  memset(&mc_size_key, 0, sizeof(mc_size_key));
  memset(&mc_size_data, 0, sizeof(mc_size_data));

  mc_size_key.data = "CACHE_SPACE_USED";
  mc_size_key.size = sizeof("CACHE_SPACE_USED");

// check "CACHE_SPACE_USED" record in db

  if (((mc_ret = dbp->get(dbp, NULL, &mc_size_key, &mc_size_data, 0)) == 0)
      && ( mc_size_data.data != NULL )
      && ( strlen(mc_size_data.data) != 0 ) )
  {

    if ( debug_level & 512 )
    {
      fprintf(stderr, "map_cache_del: %s: key retrieved: data was %s.\n",
              (mc_size_key.data == NULL) ? "(null)" : (char *)mc_size_key.data,
              (mc_size_data.data == NULL) ? "(null)" : (char *)mc_size_data.data);
    }

    set_dangerous("map_cache_del: atoi");
    if (mc_size_data.data == NULL)
    {
      mc_space_used = 0;
    }
    else
    {
      mc_space_used = atoi( (char *)mc_size_data.data);
    }
    clear_dangerous();

    if ( debug_level & 512 )
    {
      fprintf (stderr, "map_cache_del: CACHE_SPACE_USED = %.2f mb\n",
               (mc_space_used/1024/1024.0));
    }
  }
  else
  {
    // Failed the "dpb->get" operation

    if (mc_size_data.data == NULL)
    {

      if ( debug_level & 512 )
      {
        fprintf (stderr, "map_cache_del: CACHE_SPACE_USED get returned null \n");
      }
    }
    else
    {

      if ( debug_level & 512 )
      {
        fprintf (stderr, "map_cache_del: Unable to check CACHE_SPACE_USED: %s\n",
                 db_strerror(mc_ret));
      }
    }

// RETURN() HERE?

  }


  if ( debug_level & 512 )
  {
    fprintf (stderr, "map_cache_del: mc_space_used before = %d bytes file_status.st_size %d\n",
             mc_space_used,
             (int) file_status.st_size);
  }

  mc_ret = unlink( mc_delete_file );

  if ( debug_level & 512 )
  {
    fprintf(stderr,"map_cache_del: file %s unlink returned:%d.\n",
            mc_delete_file,
            mc_ret);
  }

  if (mc_ret != 0 )
  {

    if ( debug_level & 512 )
    {
      fprintf (stderr, "map_cache_del: unlink failed mc_space_used = %d bytes \n",
               mc_space_used);
    }
    return(mc_ret);
  }


// Update cache_space_used

// setup

  mc_space_used -= (int) file_status.st_size;

  if (mc_space_used < 0)
  {
    mc_space_used = 0;
  }

  if ( debug_level & 512 )
  {
    fprintf (stderr, "map_cache_del: unlink succeeded mc_space_used = %d bytes \n",
             mc_space_used);
  }

  memset(&mc_size_key, 0, sizeof(mc_size_key));
  memset(&mc_size_data, 0, sizeof(mc_size_data));

// data

  mc_size_key.data = "CACHE_SPACE_USED";
  mc_size_key.size = sizeof("CACHE_SPACE_USED");

  xastir_snprintf(mc_buf, sizeof(mc_buf), "%d", mc_space_used);

  if ( debug_level & 512 )
  {
    fprintf (stderr, "map_cache_del: mc_buf: %s len %d\n",
             mc_buf, (int)sizeof(mc_buf));
  }

  mc_size_data.data = mc_buf ;
  mc_size_data.size = sizeof(mc_buf);

// put

  if ((mc_ret = dbp->put(dbp, NULL, &mc_size_key, &mc_size_data, 0)) != 0)
  {
    // Failed the "dbp->put" operation

    if ( debug_level & 512 )
    {
      dbp->err(dbp, mc_ret, "DB->put");
    }

    // db_strerror(mc_ret);

// RETURN() HERE?

  }

  if ( debug_level & 512 )
  {
    fprintf(stderr,"map_cache_del: %s: key stored.\n",
            (mc_size_key.data == NULL) ? "(null)" : (char *)mc_size_key.data);
  }


  // remove entry from cache url->filename database

  if ((mc_ret = dbp->del(dbp, NULL, &mc_key, 0)) != 0)
  {
    // Failed the "dbp->del" operation

    if ( debug_level & 512 )
    {
      dbp->err(dbp, mc_ret, "DB->del");
    }
    // db_strerror(mc_ret);

// RETURN() HERE?

  }

  if ( debug_level & 512 )
  {
    fprintf(stderr, "map_cache_del: %s: key was deleted.\n",
            (mc_key.data == NULL) ? "(null)" : (char *)mc_key.data);
  }

  // close  the db.

  // Only try the close if we have a valid handle
  if (dbp != NULL)
  {

    set_dangerous("map_cache_del: dbp->close 2");
    if ((mc_t_ret = dbp->close(dbp, 0)) != 0 && mc_ret == 0)
    {
      clear_dangerous();

      mc_ret = mc_t_ret;
      // db_strerror(mc_ret);

// RETURN() HERE?

    }
  }

  return (0); // All is well

  /** end map_cache_del **/
}





char * map_cache_fileid(void)
{

  // returns a unique identifier
  // used for generating filenames for cached files

  time_t t;
  char * mc_time_buf;


  mc_time_buf = malloc (16);
  sprintf( mc_time_buf, "%d",(int) time(&t));
  return (mc_time_buf);
}





// check for files old enough to expire
// this is lame but it should work for now.
// expects file name like /home/brown/.xastir/map_cache/map_1100052372.gif
//
// writing this proved a good example of why pointer arithmetic is tricky
// and a good example of why I should avoid it - n8ysz 20041110
//
int map_cache_expired( char * mc_filename, time_t mc_max_age )
{
  time_t mc_t,mc_file_age;
  char *mc_filename_tmp, *mc_time_buf_tmp, *mc_time_buf;


  if (map_cache_disabled)
  {
    return(0);
  }

  mc_time_buf=malloc(MAX_FILENAME);
  mc_time_buf_tmp=mc_time_buf;

  // grab filename
  mc_filename_tmp=strrchr(mc_filename,'/');

  // clean up map_
  mc_filename_tmp=strchr(mc_filename_tmp,'_');
  ++mc_filename_tmp;

  // save up to .gif
  while ((*mc_time_buf_tmp++ = *mc_filename_tmp++) != '.' )
    ;

  *(--mc_time_buf_tmp) ='\0';

//    if ( debug_level & 512 ) {
//          fprintf(stderr, "map_cache_expired: mc_filename is: %s mc_time_buf is: %s.\n", mc_filename, mc_time_buf);
//      }

  mc_file_age=(time(&mc_t) - ((time_t) atoi(mc_time_buf)) );

  if ( mc_file_age <  mc_max_age )
  {
    if ( debug_level & 512 )
    {
      fprintf(stderr, "map_cache_expired: mc_filename %s is NOT expired. mc_time_buf is: %s.\n",
              (mc_filename == NULL) ? "(null)" : mc_filename,
              (mc_time_buf == NULL) ? "(null)" : mc_time_buf);
    }
    free(mc_time_buf);
    return (0);
  }
  else
  {
    if ( debug_level & 512 )
    {
      fprintf(stderr, "map_cache_expired: mc_filename %s IS expired. mc_time_buf is: %s.\n",
              (mc_filename == NULL) ? "(null)" : mc_filename,
              (mc_time_buf == NULL) ? "(null)" : mc_time_buf);
    }
    free(mc_time_buf);
    return ((int)mc_file_age);
  }


  // sprintf( mc_time_buf, "%d",(int)

  return (0);
}





// Functions that need writing

int mc_check_space_used (void)
{

  if (map_cache_disabled)
  {
    return(0);
  }

  return(0);
}





int mc_update_space_used (void)
{

  if (map_cache_disabled)
  {
    return(0);
  }

  return(0);
}


#endif // USE_MAP_CACHE


