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



// Need this one before the #ifdef in order to get the definition of
// USE_MAP_CACHE, if defined.
#include "config.h"

#ifdef  USE_MAP_CACHE
#warning USE_MAP_CACHE Defined (and there was much rejoicing) 

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
#include "maps.h" 
#include "map_cache.h" 
#include <db.h>




int map_cache_put( char * map_cache_url, char * map_cache_file ){

    char mc_database_filename[MAX_FILENAME]; 
    int mc_ret, mc_t_ret;
    DBT putkey, putdata ; 
    DB *dbp; 


    xastir_snprintf(mc_database_filename,
    sizeof(mc_database_filename),
    "%s/map_cache.db", 
    get_user_base_dir("map_cache"));

// Create handle 

    if ((mc_ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr, "map_cache db_create:%s\n", db_strerror(mc_ret)); 
        return(1);
    }

// open the db 

#if  (DB_VERSION_MAJOR<4)   /** DB_VERSION Check **/
#error DB_VERSION_MAJOR < 4 

#elif (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR<=0 )

    if ((mc_ret = dbp->open(dbp,
            mc_database_filename , NULL, DB_CREATE, DB_BTREE, 0664)) != 0) {

        dbp->err(dbp, mc_ret, "%s", mc_database_filename);
        db_strerror(mc_ret); 
    }
#elif	 (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR>=1 )

    if ((mc_ret = dbp->open(dbp,
            NULL,mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0) {

        dbp->err(dbp, mc_ret, "%s", mc_database_filename);
        db_strerror(mc_ret); 
    }

#endif  /** DB_VERSION Check **/

// Before we put something in we need to see if we got space

// if mc_cache_size_limit
// db get stat 
// check number of records (which means space on disk) 
// if over limit
// delete oldest (warning feeped creature )




// Setup 

    memset(&putkey, 0, sizeof(putkey));
    memset(&putdata, 0, sizeof(putdata));

// Real data at last 

    putkey.data = map_cache_url;
    putkey.size = strlen(map_cache_url); 
    putdata.data = map_cache_file;
    
    debug_level && printf ("len map_cache_file = %d\n",strlen(map_cache_file));
    putdata.size = strlen(map_cache_file)+1; /* +1 includes  \0 */ 

// do the actual put 

    if ((mc_ret = dbp->put(dbp, NULL, &putkey, &putdata, 0)) == 0) {
        debug_level && printf("db: %s: key stored.\n", (char *)putkey.data);
    } else {
        dbp->err(dbp, mc_ret, "DB->put");
        db_strerror(mc_ret); 
    } 

    if ((mc_t_ret = dbp->close(dbp, 0)) != 0 && mc_ret == 0) {
        mc_ret = mc_t_ret;
        db_strerror(mc_ret); 
    }

/* end map_cache_put */

    return (0) ; 
}





int map_cache_get( char * map_cache_url, char * map_cache_file ){

    DBT mc_key, mc_data ; 
    DB *dbp;
    int mc_ret, mc_t_ret, mc_file_stat ;
    char mc_database_filename[MAX_FILENAME]; 
    struct stat file_status;


    xastir_snprintf(mc_database_filename,
        sizeof(mc_database_filename),   // change to max_filename?
        "%s/map_cache.db",
        get_user_base_dir("map_cache"));
    
    if ((mc_ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr, "map_cache db_create:%s\n", db_strerror(mc_ret)); 
        return(1);
    }

#if  (DB_VERSION_MAJOR<4)   /** DB_VERSION Check **/
#error DB_VERSION_MAJOR < 4 

#elif (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR<=0 )

    if ((mc_ret = dbp->open(dbp,
        mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0) {
        dbp->err(dbp, mc_ret, "%s", mc_database_filename);
        db_strerror(mc_ret);
    }

#elif	 (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR>=1 )
	
    if ((mc_ret = dbp->open(dbp,
        NULL,mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0) {
        dbp->err(dbp, mc_ret, "%s", mc_database_filename);
        db_strerror(mc_ret);
    }

#endif  /** DB_VERSION Check **/

    memset(&mc_key, 0, sizeof(mc_key));
    memset(&mc_data, 0, sizeof(mc_data));

    mc_key.data=map_cache_url ; 
    mc_key.size=strlen(map_cache_url); 
	
    if ((mc_ret = dbp->get(dbp, NULL, &mc_key, &mc_data, 0)) == 0) {
        debug_level && printf("map_cache_get: %s: key retrieved: data was %s.\n",
                            (char *)mc_key.data, (char *)mc_data.data);
        xastir_snprintf(map_cache_file, MAX_FILENAME, "%s",(char *)mc_data.data);

    // check age of file delete if too old
        
        if ( (mc_ret=map_cache_expired(map_cache_file, (MC_MAX_FILE_AGE)))){
            debug_level && printf("map_cache_get: deleting expired key: %s.\n\tDeleting File: %s",
                                (char *)mc_key.data,
                                (char *)mc_data.data); 
            map_cache_del(map_cache_url);
            return (mc_ret);
        } 

    // check if the file exists 
        debug_level && fprintf(stderr,"map_cache_get: checking for existance of map_cache_file:  %s.\n",
                            map_cache_file);

        mc_file_stat=stat(map_cache_file, &file_status);

        debug_level && fprintf(stderr,"map_cache_get: map_cache_file %s stat returned:%d.\n",
                            map_cache_file,
                            mc_file_stat);

        if ( mc_file_stat == -1 ) {
	// 		
            debug_level && fprintf(stderr,"map_cache_get: attempting delete map_cache_file %s \n",
                                map_cache_file);
            if ((mc_ret = dbp->del(dbp, NULL, &mc_key, 0)) == 0) {
                debug_level && fprintf(stderr, "map_cache_get: File stat failed %s: key was deleted.\n",
                                    (char *)mc_key.data);
            }
            else {
                dbp->err(dbp, mc_ret, "DB->del");
                db_strerror(mc_ret);
            }		
            if ((mc_t_ret = dbp->close(dbp, 0)) != 0 && mc_ret == 0){
                mc_ret = mc_t_ret;
                db_strerror(mc_ret);
            }
  
            db_strerror(mc_ret);
            // Return the file stat if there was a file problem
            return (mc_file_stat);
        } else {
            if ((mc_t_ret = dbp->close(dbp, 0)) != 0 && mc_ret == 0){
                mc_ret = mc_t_ret;
                db_strerror(mc_ret);
            }
            // If we made it here all is good
            return (0); 
        }
		
    } else {
        dbp->err(dbp, mc_ret, "DB->get");
        db_strerror(mc_ret);
        // there was some problem getting things from the db
        // return the return from the get 
        
        return (mc_ret); 
    }
/** end map_cache_get **/  
}





int map_cache_del( char * map_cache_url ){

    // Delete entry from the cache 
   
    DBT mc_key, mc_data ; 
    DB *dbp;
    int mc_ret, mc_t_ret;
    char mc_database_filename[MAX_FILENAME]; 


    xastir_snprintf(mc_database_filename,
        MAX_FILENAME,
        "%s/map_cache.db",
        get_user_base_dir("map_cache"));

    if ((mc_ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr, "map_cache_del db_create:%s\n", db_strerror(mc_ret)); 
        return(1);
    }

#if  (DB_VERSION_MAJOR<4)   /** DB_VERSION Check **/
#error DB_VERSION_MAJOR < 4 

#elif (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR<=0 )

    if ((mc_ret = dbp->open(dbp,
        mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0) {
        dbp->err(dbp, mc_ret, "%s", mc_database_filename);
        db_strerror(mc_ret);
    }
#elif	 (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR>=1 )
	
    if ((mc_ret = dbp->open(dbp,
        NULL,mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0) {
        dbp->err(dbp, mc_ret, "%s", mc_database_filename);
        db_strerror(mc_ret);
    }

#endif  /** DB_VERSION Check **/

    memset(&mc_key, 0, sizeof(mc_key));
    memset(&mc_data, 0, sizeof(mc_data));
	
    mc_key.data=map_cache_url ; 
    mc_key.size=strlen(map_cache_url); 

    // Try to get the key from the cache
	
    if ((mc_ret = dbp->get(dbp, NULL, &mc_key, &mc_data, 0)) == 0) {
        debug_level && printf("map_cache_del: %s: key retrieved: data was %s.\n",
                            (char *)mc_key.data,
                            (char *)mc_data.data);

    // cleanup file on disk
	
        unlink( mc_data.data  );        
	
    // remove entry from cache
	
        if ((mc_ret = dbp->del(dbp, NULL, &mc_key, 0)) == 0){
            debug_level && fprintf(stderr, "map_cache_del: %s: key was deleted.\n",
                                (char *)mc_key.data);
        } 
        else {
            dbp->err(dbp, mc_ret, "DB->del");
            db_strerror(mc_ret);
        }							

    // close  the db. 
	
        if ((mc_t_ret = dbp->close(dbp, 0)) != 0 && mc_ret == 0){
            mc_ret = mc_t_ret;
            db_strerror(mc_ret);
        }

    return (0); 
		
    } else {
        dbp->err(dbp, mc_ret, "DB->del");
        db_strerror(mc_ret);
        return (mc_ret); 
    }

/** end map_cache_del **/  
}





char * map_cache_fileid(){

    // returns a unique identifier 
    // used for generating filenames for cached files

    time_t t; 
    char * mc_time_buf;


    mc_time_buf = malloc (16); 
    sprintf( mc_time_buf, "%d",(int) time(&t)); 
    return (mc_time_buf);
}





int map_cache_expired( char * mc_filename, time_t mc_max_age ){

    // check for files old enough to expire 
    // this is lame but it should work for now. 
    // expects file name like /home/brown/.xastir/map_cache/map_1100052372.gif

    // writing this proved a good example of why pointer arithmetic is tricky
    // and a good example of why I should avoid it - n8ysz 20041110
    
    time_t mc_t,mc_file_age; 
    char *mc_filename_tmp, *mc_time_buf_tmp, *mc_time_buf;
    
    
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

//    debug_level && fprintf(stderr, "map_cache_expired: mc_filename is: %s mc_time_buf is: %s.\n", mc_filename, mc_time_buf);

    mc_file_age=(time(&mc_t) - ((time_t) atoi(mc_time_buf)) ); 
    
    if ( mc_file_age <  mc_max_age ) {
        debug_level && fprintf(stderr, "map_cache_expired: mc_filename %s is NOT expired. mc_time_buf is: %s.\n",
                            mc_filename,
                            mc_time_buf);
        free(mc_time_buf); 
        return (0); 
    } else {
        debug_level && fprintf(stderr, "map_cache_expired: mc_filename %s IS expired. mc_time_buf is: %s.\n",
                            mc_filename,
                            mc_time_buf);        
        free(mc_time_buf); 
        return ((int)mc_file_age); 
    }


   // sprintf( mc_time_buf, "%d",(int)

    return (0);
}

#endif  // USE_MAP_CACHE


